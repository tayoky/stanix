#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/ringbuf.h>
#include <kernel/input.h>
#include <kernel/time.h>
#include <kernel/string.h>
#include <kernel/arch.h>
#include <module/ps2.h>
#include <poll.h>
#include <input.h>
#include <errno.h>

#define PS2_SET_SCANCODE_SET 0xF0

static device_driver_t ps2_kb_driver;

static void keyboard_handler(fault_frame *frame, void *arg){
	(void)frame;
	input_device_t *keyboard = arg;

	uint8_t scancode = ps2_read();

	int extended = 0;
	if(scancode == 0xE0){
		extended = 1;
		scancode = ps2_read();
	}

	int press = 1;
	if(scancode & 0x80){
		scancode &= ~0x80;
		press = 0;
	}

	struct input_event event;
	memset(&event,0,sizeof(struct input_event));
	event.timestamp = time;
	event.ie_class = IE_CLASS_KEYBOARD;
	event.ie_subclass = IE_SUBCLASS_PS2_KBD;
	event.ie_type = IE_KEY_EVENT;
	if(press){
		event.ie_key.flags = IE_KEY_PRESS;
	} else {
		event.ie_key.flags = IE_KEY_RELEASE;
	}

	event.ie_key.scancode = extended ? scancode + 0x80 : scancode;
	send_input_event(keyboard, &event);
}

//helper
#define CHANGE_SCANCODE(set) \
	ps2_send(port,PS2_SET_SCANCODE_SET);\
	if(ps2_send(port,set) != PS2_ACK){\
		kdebugf("error while changing scancode\n");\
		return -EIO;\
	}
#define GET_SCANCODE() \
	ps2_send(port,PS2_SET_SCANCODE_SET);\
	if(ps2_send(port,0) != PS2_ACK){\
		kdebugf("error while reading scancode\n");\
		return -EIO;\
	}

static int kb_check(bus_addr_t *addr) {
	ps2_addr_t *ps2_addr = (ps2_addr_t*)addr;
	if (addr->type != BUS_PS2) return 0;

	switch (ps2_addr->device_id[0]) {
	case 0xAB:
	case -1:
		kdebugf("ps2 keyboard found on port %d\n", ps2_addr->port);
		return 1;
	default:
		return 0;
	}
}

static int kb_probe(bus_addr_t *addr) {
	ps2_addr_t *ps2_addr = (ps2_addr_t*)addr;
	int port = ps2_addr->port;

	//start by reset the device
	if(ps2_send(port,0xFF) != PS2_ACK){
		kdebugf("ps2 : failed to reset device\n");
		return -EIO;
	}
	if(ps2_read() != 0xAA){
		kdebugf("ps2 : keyboard didn't pass self test\n");
		return -EIO;
	}
	// discard the id of the keyboard we aready know that
	ps2_read();
	ps2_read();


	// set scancode 2 and keep it if translation enable
	CHANGE_SCANCODE(2);
	GET_SCANCODE();
	if(ps2_read() == 0x41){
		kdebugf("ps2 : using translation\n");
	} else {
		// tranlation not enabled so set scancode 1
		CHANGE_SCANCODE(1)

		// check it's actually using scancode 1
		GET_SCANCODE();
		if(ps2_read() != 1){
			kdebugf("ps2 : device don't support scancode set 1\n");
			return -ENOTSUP;
		}
	}

	if(ps2_send(port,PS2_ENABLE_SCANING) != PS2_ACK){
		kdebugf("ps2 : error while enabling scaning\n");
		return -EIO;
	}

	kdebugf("keyboard succefuly initialized creating device\n");
	input_device_t *keyboard = kmalloc(sizeof(input_device_t));
	memset(keyboard, 0, sizeof(input_device_t));
	keyboard->device.driver = &ps2_kb_driver;
	keyboard->device.number = ps2_addr->port;
	keyboard->device.name   = strdup("kb0");
	keyboard->device.addr   = addr;
	register_input_device(keyboard);

	ps2_register_handler(keyboard_handler,port,keyboard);

	return 0;
}

static device_driver_t ps2_kb_driver = {
	.name = "ps2 keyboard",
	.check = kb_check,
	.probe = kb_probe,
};

static int init_ps2kb(int argc,char **argv){
	(void)argc;
	(void)argv;
	register_device_driver(&ps2_kb_driver);
	return 0;
}

static int fini_ps2kb(){
	unregister_device_driver(&ps2_kb_driver);
	return 0;
}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = init_ps2kb,
	.fini = fini_ps2kb,
	.name = "ps2 keyboard",
	.description = "driver for ps2 keyboard",
	.author = "tayoky",
	.license = "GPL 3"
};
