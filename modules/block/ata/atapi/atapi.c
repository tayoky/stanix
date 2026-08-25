#include <kernel/userspace.h>
#include <kernel/module.h>
#include <kernel/block.h>
#include <kernel/kheap.h>
#include <kernel/bus.h>
#include <module/scsi.h>
#include <module/ata.h>

#define ATAPI_SIG 0xeb140101

typedef struct atapi_disk {
	ata_common_ident_t common_ident;
	size_t packet_length;
} atapi_disk_t;

static int atapi_check(devnode_t *devnode) {
	ata_device_t *device = container_of(devnode, ata_device_t, devnode);
	return device->signature == ATAPI_SIG;
}

static int atapi_probe(devnode_t *devnode) {
	ata_device_t *device = container_of(devnode, ata_device_t, devnode);

	ata_ident_t ident;
	ata_command_t *identify = ata_create_command(device);
	identify->regs.command = ATA_CMD_IDENTIFY_PACKET;
	identify->flags        = ATA_CMD_READ_BUF;
	identify->buf          = &ident;
	identify->buf_size     = &ident;

	int ret = ioreq_submit_sync(&identify->ioreq);
	if (ret < 0) return ret;

	atapi_disk_t *disk = kmalloc(sizeof(atapi_disk_t));
	if (!disk) return -ENOMEM;
	memset(disk, 0, sizeof(atapi_disk_t));
	ata_parse_common_ident(&disk->common_ident, &ident);

	// which size is a packet?
	switch (ident.flags & ATA_IDENT_FLAG_PACKET_SIZE) {
	case ATA_IDENT_FLAG_PACKET_SIZE_12BYTE:
		disk->packet_length = 12;
		break;
	case ATA_IDENT_FLAG_PACKET_SIZE_16BYTE:
		disk->packet_length = 16;
		break;
	default:
		kwarning("unknow packet size\n");
		kfree(disk);
		return -ENOTSUP;
	}

	devnode->private = disk;

	// spawn SCSI
	scsi_create_device(devnode);
	return 0;
}

static void atapi_detach(devnode_t *devnode) {
	kfree(devnode->private);
}

static void atapi_command_finish(ioreq_t *ioreq, void *data) {
	// bubble up
	ioreq_finish(data, ioreq->ret);
}

static int atapi_send_scsi_command(devnode_t *devnode, scsi_device_t *scsi_device, scsi_command_t *command) {
	(void)scsi_device;
	ata_device_t *device = container_of(devnode, ata_device_t, devnode);
	atapi_disk_t *disk = devnode->private;

	// package the scsi command inside a PACKET ata command
	ata_command_t *ata_command = ata_create_command(device);
	if (!ata_command) return -ENOMEM;

	ata_command->regs.command = ATA_CMD_PACKET;
	ata_command->regs.flags   = ATA_CMD_PACKET_PROTOCOL | ATA_CMD_SEND_LBA28;
	// set the maximum bytes count
	ata_command->regs.lba1 = (uint8_t)(command->buf_size >> 0);
	ata_command->regs.lba2 = (uint8_t)(command->buf_size >> 8);

	memcpy(&ata_command->packet, &command->data, sizeof(ata_command->packet));
	ata_command->packet_length  = disk->packet_length;
	ata_command->buf_size = command->buf_size;
	ata_command->buf      = command->size;
	if (command->flags & SCSI_CMD_READ_BUF) {
		ata_command->flags |= SCSI_CMD_READ_BUF;
	} else if (command->flags & SCSI_CMD_WRITE_BUF) {
		ata_command->flags |= SCSI_CMD_WRITE_BUF;
	}

	ioreq_set_callback(&ata_command->ioreq, command);

	return ioreq_submit(&ata_command->ioreq);
}

static scsi_driver_t atapi_driver = {
	.driver = {
		.name = "ATAPI disk",
		.device_name = "scsi_bus",
		.buses = ATA_BUSES,
		.check  = atapi_check,
		.probe  = atapi_probe,
		.detach = atapi_detach,
	},
	.send_scsi_command = atapi_send_scsi_command,
};

static int atapi_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	return driver_register(&atapi_driver.driver);
}

static int atapi_fini(void) {
	return driver_unregister(&atapi_driver.driver);
}

kmodule_t module_meta = {
	.magic       = MODULE_MAGIC,
	.init        = ata_init,
	.fini        = ata_fini,
	.author      = "tayoky",
	.name        = "atapi",
	.description = "ATAPI disk driver",
	.license     = "GPL 3",
};
