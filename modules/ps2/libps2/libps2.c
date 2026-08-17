#include <kernel/module.h>
#include <module/ps2.h>

int ps2_add_device(devnode_t *controller, ps2_dev_t *device) {
	device->controller = controller;
	if (ps2_send_command(device, PS2_DISABLE_SCANNING) != PS2_ACK) {
		// no or broken device
		return -ENODEV;
	}

	kinfof("found device : ");

	if (ps2_send_command(device, PS2_IDENTIFY) != PS2_ACK) {
		kprintf("unknown device\n");
		return 0;
	}

	int c0 = ps2_read(device);
	int c1 = -1;
	if (c0 == 0xAB || c0 == 0xAC) {
		c1 = ps2_read(device);
	}

	device->device_id[0] = c0;
	device->device_id[1] = c1;

	switch (c0) {
	case -1: //-1 mean no byte
		kprintf("Ancient AT keyboard\n");
		break;
	case 0x00:
		kprintf("Standard PS/2 mouse\n");
		break;
	case 0x03:
		kprintf("Mouse with scroll wheel\n");
		break;
	case 0x04:
		kprintf("5-button mouse\n");
		break;
	case 0xAB:
		switch (c1) {
		case 0x83:
		case 0xC1:
			kprintf("MF2 keyboard\n");
			break;
		case 0x84:
			kprintf("Short Keyboard\n");
			break;
		case 0x85:
			kprintf("122-Key Host Connect(ed) Keyboard\n");
			break;
		case 0x86:
			kprintf("122-key keyboards\n");
			break;
		default:
			kprintf("unknown keyboard %x:%x\n", c0, c1);
			break;
		}
		break;
	case 0xAC:
		switch (c1) {
		case 0xA1:
			kprintf("NCD Sun layout keyboard\n");
			break;
		default:
			kprintf("unknown device\n");
			break;
		}
		break;
	default:
		kprintf("unknown device : %x:%x\n", c0, c1);
		break;
	}

	bus_attach_child(controller, &device->devnode, NULL, UNIT_NOUNIT);
	return 0;
}

int ps2_reset(ps2_dev_t *device) {
	int ret = ps2_send_command(device, PS2_RESET);
	if (ret < 0)  return ret;
	if (ret != PS2_ACK) return -EIO;

	ret = ps2_read(device);
	if (ret < 0) return ret;
	if (ret != PS2_SELF_TEST_PASSED) return -EIO;

	// discard the id
	int c0 = ps2_read(device);
	if (c0 == 0xAB || c0 == 0xAC) {
		ps2_read(device);
	}
	return 0;
}

int libps2_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	EXPORT(ps2_add_device);
	EXPORT(ps2_reset);
	return 0;
}

int libps2_fini(void) {
	UNEXPORT(ps2_add_device);
	UNEXPORT(ps2_reset);
	return 0;
}

kmodule_t module_meta = {
	.magic = MODULE_MAGIC,
	.init = libps2_init,
	.fini = libps2_fini,
	.name = "ps2 utilities library",
	.author = "tayoky"
};
