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

#define PS2_MOUSE_SET_RATE 0xF3

typedef struct ps2_mouse {
	input_device_t input_device;
	int button;
	int packet;
	int flags;
	int y;
	int x;
} ps2_mouse_t;

static device_driver_t ps2_mouse_driver;

static int ps2_mouse_set_rate(int port, int rate) {
	if (ps2_send(port, PS2_MOUSE_SET_RATE) != PS2_ACK) return -EIO;
	if (ps2_send(port, rate) != PS2_ACK) return -EIO;
	return 0;
}

static void ps2_mouse_handler(registers_t *registers, void *data) {
	(void)registers;
	ps2_mouse_t *mouse = data;
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
		// overflow so just ignore
		return;
	}

	// did buttons change
	if ((mouse->flags & 0x7) != mouse->button) {
		kdebugf("button %d %d %d\n", mouse->flags & 2, mouse->flags & 4, mouse->flags & 1);
		int change = (mouse->flags & 0x07) ^ mouse->button;
		for (int i = 0; i < 3; i++) {
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
			input_device_send_event((input_device_t *)mouse, &event);
		}
		mouse->button = mouse->flags & 0x7;
	}

	int x = mouse->x;
	int y = mouse->y;

	if (mouse->flags & (1 << 4)) {
		x = x - 256;
	}

	if (mouse->flags & (1 << 5)) {
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
		input_device_send_event((input_device_t *)mouse, &event);
	}
}

static int ps2_mouse_check(bus_addr_t *addr) {
	ps2_addr_t *ps2_addr = (ps2_addr_t *)addr;
	if (addr->type != BUS_PS2) return 0;
	if (ps2_addr->device_id[0]) return 0;
	kdebugf("found ps2 mouse on port %d\n", ps2_addr->port);
	return 1;
}

static int ps2_mouse_probe(bus_addr_t *addr) {
	ps2_addr_t *ps2_addr = (ps2_addr_t *)addr;
	int port = ps2_addr->port;

	// first do a reset
	if (ps2_reset(port) < 0) {
		kinfof("mouse reset failed\n");
		return -EIO;
	}

	if (ps2_send(port, PS2_ENABLE_SCANNING) != PS2_ACK) {
		kinfof("error while enabling scanning\n");
		return -EIO;
	}

	ps2_mouse_t *mouse = kmalloc(sizeof(ps2_mouse_t));
	memset(mouse, 0, sizeof(ps2_mouse_t));
	mouse->input_device.device.number = port;
	mouse->input_device.device.driver = &ps2_mouse_driver;
	mouse->input_device.device.name = strdup("mouse0");
	mouse->input_device.device.addr = addr;
	mouse->input_device.class = IE_CLASS_MOUSE;
	mouse->input_device.subclass = IE_SUBCLASS_PS2_MOUSE;
	input_device_register(&mouse->input_device);
	bus_register_handler(addr, ps2_mouse_handler, mouse);
	return 0;
}

static device_driver_t ps2_mouse_driver = {
	.name = "ps2 mouse",
	.check = ps2_mouse_check,
	.probe = ps2_mouse_probe,
};

int init_ps2_mouse(int argc, char **argv) {
	(void)argc;
	(void)argv;
	device_driver_register(&ps2_mouse_driver);
	return 0;
}

int fini_ps2_mouse() {
	device_driver_unregister(&ps2_mouse_driver);
	return 0;
}


kmodule_t module_meta = {
	.magic = MODULE_MAGIC,
	.init = init_ps2_mouse,
	.fini = fini_ps2_mouse,
	.author = "tayoky",
	.name = "ps2 mouse",
	.description = "a ps2 mouse driver",
	.license = "GPL 3",
};
