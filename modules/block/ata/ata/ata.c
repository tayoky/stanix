#include <kernel/userspace.h>
#include <kernel/module.h>
#include <kernel/block.h>
#include <kernel/kheap.h>
#include <kernel/bus.h>
#include <module/ata.h>
#include <sys/block.h>
#include <sys/ioctl.h>

#define ATA_SIG 0x00000101

typedef struct ata_disk {
	block_device_t block_device;
	ata_common_ident_t common_ident; 
} ata_disk_t;

static void ata_finish_callback(ioreq_t *ioreq, void *data) {
	block_finish_request(data, ioreq->ret);
}

static int ata_submit(block_device_t *block_device, block_request_t *request) {
	ata_device_t *device = container_of(block_device->device.devnode, ata_device_t, devnode);
	ata_disk_t *disk     = container_of(block_device, ata_disk_t, block_device);

	if (request->type == BLOCK_REQUEST_FLUSH) {
		ata_command_t *command = ata_create_command(device);
		command->opcode = (disk->common_ident.command_sets & (1 << 26)) ? ATA_CMD_CACHE_FLUSH : ATA_CMD_CACHE_FLUSH_EXT;

		ioreq_set_callback(&command->ioreq, ata_finish_callback, request);
		return ioreq_submit(&command->ioreq);
	}

	// LBA28 has a lower limit
	if (request->start_sector >= 0x10000000 && !(disk->common_ident.command_sets & (1 << 26))) {
		// high LBA but no LBA28 support... uh ?
		return -EIO;
	}

	uint8_t opcode;
	int flags = 0;
	if (disk->common_ident.command_sets & (1 << 26)) {
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
	if (request->type == BLOCK_REQUEST_WRITE) {
		flags |= ATA_CMD_WRITE_BUF;
	} else {
		flags |= ATA_CMD_READ_BUF;
	}

	ata_command_t *command = ata_create_command(device);
	command->opcode = opcode;
	command->sectors_count = request->sectors_count;
	command->lba = request->start_sector;
	command->flags = flags;
	command->buf   = request->buf;
	ioreq_set_callback(&command->ioreq, ata_finish_callback, request);
	return ioreq_submit(&command->ioreq);
}

static int ata_ioctl(block_device_t *block_device, long req, void *arg) {
	if (device_is_unplugged(&block_device->device)) {
		return -ENXIO;
	}
	ata_disk_t *disk = container_of(block_device, ata_disk_t, block_device);
	switch (req) {
	case I_MODEL:
		return safe_copy_auto_to(arg, disk->common_ident.model);
	default:
		return -EINVAL;
	}
}

static void ata_cleanup(block_device_t *block_device) {
	ata_disk_t *disk = container_of(block_device, ata_disk_t, block_device);
	kfree(disk);
}

static block_ops_t ata_ops = {
	.submit  = ata_submit,
	.ioctl   = ata_ioctl,
	.cleanup = ata_cleanup,
};

static int ata_check(devnode_t *devnode) {
	ata_device_t *device = container_of(devnode, ata_device_t, devnode);
	return device->signature == ATA_SIG;
}

static int ata_probe(devnode_t *devnode) {
	ata_device_t *device = container_of(devnode, ata_device_t, devnode);

	ata_ident_t ident;
	ata_command_t *identify = ata_create_command(device);
	identify->opcode = ATA_CMD_IDENTIFY;
	identify->lba = 0;
	identify->sectors_count = 1;
	identify->flags = ATA_CMD_SEND_LBA28 | ATA_CMD_READ_BUF;
	identify->buf = &ident;

	int ret = ata_submit_command_sync(identify);
	if (ret < 0) return ret;

	ata_disk_t *disk = kmalloc(sizeof(ata_disk_t));
	if (!disk) return -ENOMEM;
	memset(disk, 0, sizeof(ata_disk_t));
	ata_parse_common_ident(&disk->common_ident, &ident);
	disk->block_device.ops = &ata_ops;
	disk->block_device.sector_size = 512;
	disk->block_device.sectors_count = disk->common_ident.sectors_count;
	disk->block_device.device.devnode = devnode;

	block_device_register(&disk->block_device, NULL, 0);
	return 0;
}

static void ata_detach(devnode_t *devnode) {
	device_destroy(devnode->device);
}

static driver_t ata_driver = {
	.name = "ATA disk",
	.device_name = "hd",
	.buses = ATA_BUSES,
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
