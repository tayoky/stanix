#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/input.h>
#include <kernel/bus.h>
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
	input_device_t *input_device;
	resource_t *irq_resource;
	void *handler_handle;
	int button;
	int packet;
	int flags;
	int y;
	int x;
} ps2_mouse_t;

static int ps2_mouse_set_rate(ps2_dev_t *ps2_dev, int rate) {
	if (ps2_send_command(ps2_dev, PS2_MOUSE_SET_RATE) != PS2_ACK) return -EIO;
	if (ps2_send_command(ps2_dev, rate) != PS2_ACK) return -EIO;
	return 0;
}

static void ps2_mouse_handler(registers_t *registers, void *data) {
	(void)registers;
	ps2_mouse_t *mouse = data;
	ps2_dev_t *ps2_dev = container_of(mouse->input_device->device.devnode, ps2_dev_t, devnode);
	
	uint8_t b = ps2_read(ps2_dev);
	switch (mouse->packet++) {
	case 0:
		if (!(b & 0x08)) {
			mouse->packet = 0;
    		return;
		}
		mouse->flags = b;
		return;
	case 1:
		mouse->x = b;
		return;
	case 2:
		mouse->y = b;
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

static int ps2_mouse_check(devnode_t *devnode) {
	ps2_dev_t *ps2_dev = container_of(devnode, ps2_dev_t, devnode);
	if (devnode->type != BUS_PS2) return 0;
	if (ps2_dev->device_id[0]) return 0;
	kdebugf("found ps2 mouse on port\n");
	return 1;
}

static int ps2_mouse_probe(devnode_t *devnode) {
	ps2_dev_t *ps2_dev = container_of(devnode, ps2_dev_t, devnode);
	ps2_mouse_t *mouse = devnode->private;

	// first do a reset
	if (ps2_reset(ps2_dev) < 0) {
		kinfof("mouse reset failed\n");
		return -EIO;
	}

	if (ps2_send_command(ps2_dev, PS2_ENABLE_SCANNING) != PS2_ACK) {
		kinfof("error while enabling scanning\n");
		return -EIO;
	}

	mouse->irq_resource = device_allocate_simple_resource(devnode, RESOURCE_IRQ, RID_ANY);
	mouse->input_device = input_device_allocate();
	mouse->input_device.device.devnode = devnode;
	mouse->input_device.class    = IE_CLASS_MOUSE;
	mouse->input_device.subclass = IE_SUBCLASS_PS2_MOUSE;
	mouse->input_device->private = mouse;
	input_device_register(mouse->input_device);
	mouse->handler_handle = resource_register_handler(mouse->irq_resource, ps2_mouse_handler, mouse);
	return 0;
}

static void ps2_mouse_detach(devnode_t *devnode) {
	ps2_mouse_t *mouse = devnode->private;
	input_device_destroy(mouse->input_device);

	resource_unregister_handler(mouse->irq_resource, mouse->handler_handle);
	device_release_resource(devnode, mouse->irq_resource);
}

static driver_t ps2_mouse_driver = {
	.name = "ps2 mouse",
	.device_name = "mouse",
	.buses = BUSES("ps2"),
	.private_size = sizeof(ps2_mouse_t),
	.check  = ps2_mouse_check,
	.probe  = ps2_mouse_probe,
	.detach = ps2_mouse_detach,
};

int init_ps2_mouse(int argc, char **argv) {
	(void)argc;
	(void)argv;
	return driver_register(&ps2_mouse_driver);
}

int fini_ps2_mouse(void) {
	return driver_unregister(&ps2_mouse_driver);
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
