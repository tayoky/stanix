#include <kernel/module.h>
#include <kernel/block.h>
#include <kernel/kheap.h>
#include <kernel/bus.h>
#include <module/ata.h>
#include <sys/block.h>
#include <sys/ioctl.h>

// TODO : real async api when the rest of the ata system suport one
static int ata_request(block_device_t *block_device, block_request_t *request) {
	ata_device_t *device = container_of(block_device->device.devnode, ata_device_t, devnode);

	// LBA28 has a lower limit
	if (request->start_sector >= 0x10000000 && !(device->command_sets & (1 << 26))) {
		// high LBA but no LBA28 support... uh ?
		return -EIO;
	}

	uint8_t opcode;
	int flags;
	if (device->command_sets & (1 << 26)) {
		if (request->type == BLOCK_REQUEST_WRITE) {
			opcode = ATA_CMD_WRITE_PIO_EXT;
		} else {
			opcode = ATA_CMD_READ_PIO_EXT;
		}
		flags = ATA_CMD_SEND_LBA48;
	} else {
		if (request->type == BLOCK_REQUEST_WRITE) {
			opcode = ATA_CMD_WRITE_PIO;
		} else {
			opcode = ATA_CMD_READ_PIO;
		}
		flags = ATA_CMD_SEND_LBA28;
	}

	ata_command_t command = {
		.opcode = opcode,
		.sectors_count = request->sectors_count,
		.lba = request->start_sector,
		.flags = flags,
		.buf   = request->buf,
	};
	int ret = ata_send_command(&device->devnode, &command);
	if (ret < 0) return ret;

	// we need to send cache flush on write
	if (request->type == BLOCK_REQUEST_WRITE) {
		ata_command_t flush_command = {
			.opcode = (device->command_sets & (1 << 26)) ? ATA_CMD_CACHE_FLUSH : ATA_CMD_CACHE_FLUSH_EXT,
		};
		ata_send_command(&device->devnode, &flush_command);
	}

	block_finish_request(request);

	return 0;
}

static int ata_ioctl(block_device_t *block_device, long req, void *arg) {
	if (device_is_unplugged(&block_device->device)) {
		return -ENXIO;
	}
	ata_device_t *device = container_of(block_device->device.devnode, ata_device_t, devnode);
	switch (req) {
	case I_MODEL:
		// UNSAFE
		strcpy(arg, device->model);
		return 0;
	default:
		return -EINVAL;
	}
}

static block_ops_t ata_ops = {
	.request = ata_request,
	.ioctl   = ata_ioctl,
};

static int ata_check(devnode_t *devnode) {
	(void)devnode;
	// TODO : check here, but what ?
	return 1;
}

static int ata_probe(devnode_t *devnode) {
	ata_device_t *device = container_of(devnode, ata_device_t, devnode);

	block_device_t *block_device = kmalloc(sizeof(block_device_t));
	if (!block_device) return -ENOMEM;
	memset(block_device, 0, sizeof(block_device_t));
	block_device->ops = &ata_ops;
	block_device->sector_size = 512;
	block_device->sectors_count = device->sectors_count;
	block_device->device.devnode = devnode;

	block_device_register(block_device, NULL, 0);
	return 0;
}

static void ata_detach(devnode_t *devnode) {
	device_destroy(devnode->device);
}

static driver_t ata_driver = {
	.name = "ATA disk",
	.device_name = "hd",
	.buses  = BUSES("ata_channel"),
	.check  = ata_check,
	.probe  = ata_probe,
	.detach = ata_detach,
};

static int ata_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	return driver_register(&ata_driver);
}

static int ata_fini(void) {
	return driver_unregister(&ata_driver);
}

kmodule_t module_meta = {
	.magic       = MODULE_MAGIC,
	.init        = ata_init,
	.fini        = ata_fini,
	.author      = "tayoky",
	.name        = "ata",
	.description = "ATA disk driver",
	.license     = "GPL 3",
};
