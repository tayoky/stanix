#include <kernel/arch.h>
#include <kernel/input.h>
#include <kernel/kheap.h>
#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/ringbuf.h>
#include <kernel/string.h>
#include <kernel/time.h>
#include <kernel/bus.h>
#include <module/ps2.h>
#include <errno.h>
#include <input.h>
#include <poll.h>

#define PS2_KEYBOARD_SET_SCANCODE 0xF0

typedef struct ps2_kb {
	input_device_t input_device;
	resource_t *irq_resource;
	void *handler_handle;
	int extended;
} ps2_kb_t;

static void ps2_kb_handler(registers_t *registers, void *data) {
	(void)registers;
	ps2_kb_t *keyboard = data;

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
	gettime(CLOCK_MONOTONIC, &event.timestamp);
	event.ie_type = IE_KEY_EVENT;
	if (press) {
		event.ie_key.flags = IE_KEY_PRESS;
	} else {
		event.ie_key.flags = IE_KEY_RELEASE;
	}

	event.ie_key.scancode = keyboard->extended ? scancode + 0x80 : scancode;
	input_device_send_event(&keyboard->input_device, &event);
	keyboard->extended = 0;
}

static int ps2_kb_set_scancode(ps2_dev_t *ps2_dev, int scancode) {
	if (ps2_send(ps2_dev->port, PS2_KEYBOARD_SET_SCANCODE) != PS2_ACK) goto error;
	if (ps2_send(ps2_dev->port, scancode) != PS2_ACK) goto error;
	return 0;

error:
	kdebugf("error while changing scancode\n");
	return -EIO;
}

static int ps2_kb_get_scancode(ps2_dev_t *ps2_dev) {
	if (ps2_send(ps2_dev->port, PS2_KEYBOARD_SET_SCANCODE) != PS2_ACK) goto error;
	if (ps2_send(ps2_dev->port, 0) != PS2_ACK) goto error;
	return ps2_read();

error:
	kdebugf("error while reading scancode\n");
	return -EIO;
}

static int ps2_kb_check(devnode_t *devnode) {
	ps2_dev_t *ps2_dev = container_of(devnode, ps2_dev_t, devnode);
	if (devnode->type != BUS_PS2) return 0;

	switch (ps2_dev->device_id[0]) {
	case 0xAB:
	case 0xAC:
	case -1:
		kdebugf("ps2 keyboard found on port %d\n", ps2_dev->port);
		return 1;
	default:
		return 0;
	}
}

static int ps2_kb_probe(devnode_t *devnode) {
	ps2_dev_t *ps2_dev   = container_of(devnode, ps2_dev_t, devnode);
	int port             = ps2_dev->port;

	// reset the device
	if (ps2_reset(port) < 0) {
		kinfof("ps2 : keyboard reset failed\n");
		return -EIO;
	}

	// set scancode 2 and keep it if translation enabled
	if (ps2_kb_set_scancode(ps2_dev, 2) < 0) return -EIO;
	int scancode = ps2_kb_get_scancode(ps2_dev);
	if (scancode < 0) return -EIO;
	if (scancode == 0x41) {
		kdebugf("ps2 : using translation\n");
	} else {
		// translation not enabled so set scancode 1
		if (ps2_kb_set_scancode(ps2_dev, 1) < 0) return -EIO;

		// check it's actually using scancode 1
		int scancode = ps2_kb_get_scancode(ps2_dev);
		if (scancode < 0) return -EIO;
		if (scancode != 1) {
			kdebugf("ps2 : device do not support scancode set 1\n");
			return -ENOTSUP;
		}
	}

	if (ps2_send(port, PS2_ENABLE_SCANNING) != PS2_ACK) {
		kdebugf("ps2 : error while enabling scanning\n");
		return -EIO;
	}

	ps2_kb_t *keyboard = kmalloc(sizeof(ps2_kb_t));
	memset(keyboard, 0, sizeof(ps2_kb_t));
	keyboard->irq_resource = device_allocate_simple_resource(devnode, RESOURCE_IRQ, RID_ANY);
	keyboard->input_device.device.devnode = devnode;
	keyboard->input_device.class          = IE_CLASS_KEYBOARD;
	keyboard->input_device.subclass       = IE_SUBCLASS_PS2_KBD;
	input_device_register(&keyboard->input_device);

	keyboard->handler_handle = resource_register_handler(keyboard->irq_resource, ps2_kb_handler, keyboard);
	kdebugf("ps2 keyboard successfully initialized\n");

	return 0;
}

static driver_t ps2_kb_driver = {
	.name  = "ps2 keyboard",
	.device_name = "kb%d",
	.buses = BUSES("ps2"),
	.check = ps2_kb_check,
	.probe = ps2_kb_probe,
};

static int init_ps2_kb(int argc, char **argv) {
	(void)argc;
	(void)argv;
	driver_register(&ps2_kb_driver);
	return 0;
}

static int fini_ps2_kb() {
	driver_unregister(&ps2_kb_driver);
	return 0;
}

kmodule_t module_meta = {
	.magic       = MODULE_MAGIC,
	.init        = init_ps2_kb,
	.fini        = fini_ps2_kb,
	.name        = "ps2 keyboard",
	.description = "driver for ps2 keyboard",
	.author      = "tayoky",
	.license     = "GPL 3"
};
