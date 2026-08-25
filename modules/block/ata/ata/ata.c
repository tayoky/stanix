#include <kernel/userspace.h>
#include <kernel/module.h>
#include <kernel/block.h>
#include <kernel/kheap.h>
#include <kernel/bus.h>
#include <module/ata.h>
#include <sys/block.h>
#include <sys/ioctl.h>

#define ATA_SIG 0x00000101
#define ATA_COMMAND_SETS_LBA48 (1U << 26)

typedef struct ata_disk {
	block_device_t block_device;
	ata_common_ident_t common_ident; 
	size_t sectors_count;
} ata_disk_t;

static void ata_finish_callback(ioreq_t *ioreq, void *data) {
	block_request_t *request = data;
	// bubble up
	ioreq_finish(&request->ioreq, ioreq->ret);
}

static int ata_submit(block_device_t *block_device, block_request_t *request) {
	ata_device_t *device = container_of(block_device->device.devnode, ata_device_t, devnode);
	ata_disk_t *disk     = container_of(block_device, ata_disk_t, block_device);

	if (request->type == BLOCK_REQUEST_FLUSH) {
		ata_command_t *command = ata_create_command(device);
		command->regs.command = (disk->common_ident.command_sets & ATA_COMMAND_SETS_LBA48) ? ATA_CMD_CACHE_FLUSH_EXT : ATA_CMD_CACHE_FLUSH;

		ioreq_set_callback(&command->ioreq, ata_finish_callback, request);
		return ioreq_submit(&command->ioreq);
	}

	// LBA28 has a lower limit
	if (request->start_sector + request->sectors_count >= 0x10000000 && !(disk->common_ident.command_sets & ATA_COMMAND_SETS_LBA48)) {
		// high LBA but no LBA48 support... uh ?
		return -EIO;
	}

	uint8_t opcode;
	ata_command_t *command;
	if (disk->common_ident.command_sets & ATA_COMMAND_SETS_LBA48) {
		if (request->sectors_count > 65536) {
			return -ENOTSUP;
		}
		if (request->type == BLOCK_REQUEST_WRITE) {
			opcode = ATA_CMD_WRITE_PIO_EXT;
		} else {
			opcode = ATA_CMD_READ_PIO_EXT;
		}
		command = ata_create_lba48_command(device, opcode, request->start_sector, request->sectors_count);
	} else {
		if (request->sectors_count > 256) {
			return -ENOTSUP;
		}
		if (request->type == BLOCK_REQUEST_WRITE) {
			opcode = ATA_CMD_WRITE_PIO;
		} else {
			opcode = ATA_CMD_READ_PIO;
		}
		command = ata_create_lba28_command(device, opcode, request->start_sector, request->sectors_count);
	}

	if (request->type == BLOCK_REQUEST_WRITE) {
		command->flags |= ATA_CMD_WRITE_BUF;
	} else {
		command->flags |= ATA_CMD_READ_BUF;
	}
	command->buf      = request->buf;
	command->buf_size = request->sectors_count * block_device->sector_size;
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
		return safe_copy_auto_to(arg, &disk->common_ident.model);
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
	identify->regs.command = ATA_CMD_IDENTIFY;
	identify->flags = ATA_CMD_READ_BUF;
	identify->buf = &ident;
	identify->buf_size = sizeof(ident);

	int ret = ioreq_submit_sync(&identify->ioreq);
	if (ret < 0) return ret;

	ata_disk_t *disk = kmalloc(sizeof(ata_disk_t));
	if (!disk) return -ENOMEM;
	memset(disk, 0, sizeof(ata_disk_t));
	ata_parse_common_ident(&disk->common_ident, &ident);
	disk->block_device.ops = &ata_ops;
	disk->block_device.sector_size = 512;
	disk->block_device.sectors_count = disk->common_ident.command_sets & ATA_COMMAND_SETS_LBA48 ? ident.sectors_lba48 : ident.sectors;
	disk->block_device.device.devnode = devnode;

	kdebugf("model : %s command sets : %x support LBA48 : %s max LBA : %zu\n", disk->common_ident.model, disk->common_ident.command_sets, disk->common_ident.command_sets & ATA_COMMAND_SETS_LBA48 ? "true" : "false", disk->block_device.sectors_count);

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
