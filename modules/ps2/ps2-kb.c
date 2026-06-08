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


typedef struct ps2_kb {
	input_device_t input_device;
	int extended;
} ps2_kb_t;

static device_driver_t ps2_kb_driver;

static void keyboard_handler(registers_t *frame, void *arg) {
	(void)frame;
	ps2_kb_t *keyboard = arg;

	uint8_t scancode = ps2_read();

	if (scancode == 0xE0) {
		keyboard->extended = 1;
		return;
	}

	int press = 1;
	if (scancode & 0x80) {
		scancode &= ~0x80;
		press = 0;
	}

	struct input_event event;
	memset(&event, 0, sizeof(struct input_event));
	event.timestamp = time;
	event.ie_type = IE_KEY_EVENT;
	if (press) {
		event.ie_key.flags = IE_KEY_PRESS;
	} else {
		event.ie_key.flags = IE_KEY_RELEASE;
	}

	event.ie_key.scancode = keyboard->extended ? scancode + 0x80 : scancode;
	send_input_event(&keyboard->input_device, &event);
	keyboard->extended = 0;
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
	ps2_addr_t *ps2_addr = (ps2_addr_t *)addr;
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
	ps2_addr_t *ps2_addr = (ps2_addr_t *)addr;
	int port = ps2_addr->port;

	//start by reset the device
	if (ps2_send(port, 0xFF) != PS2_ACK) {
		kdebugf("ps2 : failed to reset device\n");
		return -EIO;
	}
	if (ps2_read() != 0xAA) {
		kdebugf("ps2 : keyboard didn't pass self test\n");
		return -EIO;
	}

	// set scancode 2 and keep it if translation enable
	CHANGE_SCANCODE(2);
	GET_SCANCODE();
	if (ps2_read() == 0x41) {
		kdebugf("ps2 : using translation\n");
	} else {
		// tranlation not enabled so set scancode 1
		CHANGE_SCANCODE(1)

			// check it's actually using scancode 1
			GET_SCANCODE();
		if (ps2_read() != 1) {
			kdebugf("ps2 : device don't support scancode set 1\n");
			return -ENOTSUP;
		}
	}

	if (ps2_send(port, PS2_ENABLE_SCANNING) != PS2_ACK) {
		kdebugf("ps2 : error while enabling scaning\n");
		return -EIO;
	}

	ps2_kb_t *keyboard = kmalloc(sizeof(ps2_kb_t));
	memset(keyboard, 0, sizeof(ps2_kb_t));
	keyboard->input_device.device.driver = &ps2_kb_driver;
	keyboard->input_device.device.number = ps2_addr->port;
	keyboard->input_device.device.name   = strdup("kb0");
	keyboard->input_device.device.addr   = addr;
	keyboard->input_device.class = IE_CLASS_KEYBOARD;
	keyboard->input_device.subclass = IE_SUBCLASS_PS2_KBD;
	register_input_device(&keyboard->input_device);
	bus_register_handler(addr, keyboard_handler, keyboard);
	kdebugf("keyboard succefuly initialized\n");

	return 0;
}

static device_driver_t ps2_kb_driver = {
	.name = "ps2 keyboard",
	.check = kb_check,
	.probe = kb_probe,
};

static int init_ps2kb(int argc, char **argv) {
	(void)argc;
	(void)argv;
	device_driver_register(&ps2_kb_driver);
	return 0;
}

static int fini_ps2kb() {
	device_driver_unregister(&ps2_kb_driver);
	return 0;
}

kmodule_t module_meta = {
	.magic = MODULE_MAGIC,
	.init = init_ps2kb,
	.fini = fini_ps2kb,
	.name = "ps2 keyboard",
	.description = "driver for ps2 keyboard",
	.author = "tayoky",
	.license = "GPL 3"
};
