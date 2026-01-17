#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/input.h>
#include <kernel/kheap.h>
#include <kernel/ringbuf.h>
#include <kernel/arch.h>
#include <kernel/string.h>
#include <kernel/time.h>
#include <module/ps2.h>
#include <errno.h>
#include <input.h>

#define PS2_SET_RATE 0xF3

typedef struct ps2_mouse {
	input_device_t device;
	int button;
	int packet;
	int flags;
	int y;
	int x;
} ps2_mouse_t;

static device_driver_t ps2_mouse_driver;

static int set_mouse_rate(int port,int rate){
	if(ps2_send(port,PS2_SET_RATE) != PS2_ACK){
		return -1;
	}
	if(ps2_send(port,rate) != PS2_ACK){
		return -1;
	}
	return 0;
}

static void mouse_handler(fault_frame *frame, void *arg){
	(void)frame;
	ps2_mouse_t *mouse = arg;
	switch (mouse->packet++) {
	case 0:
		mouse->flags = ps2_read();
		return;
	case 1:
		mouse->x = ps2_read();
		return;
	case 2:
		mouse->y = ps2_read();
		mouse->packet = 0;
		break;
	}

	if (mouse->flags & 0xC0) {
		// overflow so just ingore
		return;
	} 

	// did buttons changes
	if ((mouse->flags & 0x7) != mouse->button) {
		kdebugf("button %d %d %d\n", mouse->flags & 2, mouse->flags & 4, mouse->flags & 1);
		int change = (mouse->flags & 0x07) ^ mouse->button;
		for (int i = 0; i<3; i++) {
			if (!(change & (1 << i))) continue;
			struct input_event event = {
				.ie_type = IE_KEY_EVENT,
			};
			switch (i) {
			case 0:
				event.ie_key.scancode = INPUT_KEY_MOUSE_LEFT;
				break;
			case 1:
				event.ie_key.scancode = INPUT_KEY_MOUSE_RIGHT;
				break;
			case 2:
				event.ie_key.scancode = INPUT_KEY_MOUSE_MIDDLE;
				break;
			}
			if (mouse->flags & 0x7 & (1 << i)) {
				event.ie_key.flags = IE_KEY_PRESS;
			} else {
				event.ie_key.flags = IE_KEY_RELEASE;
			}
			send_input_event((input_device_t*)mouse, &event);
		}
		mouse->button = mouse->flags & 0x7;
	}

	int x = mouse->x;
	int y = mouse->y;

	if(mouse->flags & (1 << 4)){
		x = x - 256;
	}

	if(mouse->flags & (1 << 5)){
		y = y - 256;
	}

	if (x != 0 || y != 0) {
		struct input_event event = {
			.ie_type = IE_MOVE_EVENT,
			.ie_move = {
				.x = x,
				.y = -y,
				.axis = 0,
			},
		};
		send_input_event((input_device_t*)mouse, &event);
	}
}

static int mouse_check(bus_addr_t *addr) {
	ps2_addr_t *ps2_addr = (ps2_addr_t*)addr;
	if (addr->type != BUS_PS2) return 0;
	if (ps2_addr->device_id[0]) return 0;
	kdebugf("found ps2 mouse on port %d\n", ps2_addr->port);
	return 1;
}

static int mouse_probe(bus_addr_t *addr) {
	ps2_addr_t *ps2_addr = (ps2_addr_t*)addr;
	int port = ps2_addr->port;

	// first do a reset
	if (ps2_send(port, 0xFF) != PS2_ACK) {
		kdebugf("mouse reset failed\n");
		return -EIO;
	}
	if (ps2_read() != 0xAA) {
		kdebugf("mouse didn't pass self test\n");
		return -EIO;
	}
	// discard the id of the mouse we aready know that
	ps2_read();
	ps2_read();

	if (ps2_send(port, PS2_ENABLE_SCANING) != PS2_ACK) {
		kdebugf("error while enabling scanning\n");
		return -EIO;
	}

 	ps2_mouse_t *mouse = kmalloc(sizeof(ps2_mouse_t));
	memset(mouse, 0, sizeof(ps2_mouse_t));
	mouse->device.device.number = port;
	mouse->device.device.driver = &ps2_mouse_driver;
	mouse->device.device.name = strdup("mouse0");
	mouse->device.device.addr = addr;
	mouse->device.class = IE_CLASS_MOUSE;
	mouse->device.subclass = IE_SUBCLASS_PS2_MOUSE;
	register_input_device((input_device_t*)mouse);
	bus_register_handler(addr, mouse_handler, mouse);
	return 0;
}

static device_driver_t ps2_mouse_driver = {
	.name = "ps2 mouse",
	.check = mouse_check,
	.probe = mouse_probe,
};

int init_mouse(int argc,char **argv){
	(void)argc;
	(void)argv;
	register_device_driver(&ps2_mouse_driver);
	return 0;
}

int fini_mouse(){
	unregister_device_driver(&ps2_mouse_driver);
	return 0;
}


kmodule_t module_meta = {
	.magic = MODULE_MAGIC,
	.init = init_mouse,
	.fini = fini_mouse,
	.author = "tayoky",
	.name = "ps2 mouse",
	.description = "a ps2 mouse driver",
	.license = "GPL 3",
};
