#include <kernel/userspace.h>
#include <kernel/module.h>
#include <kernel/block.h>
#include <kernel/kheap.h>
#include <kernel/bus.h>
#include <module/scsi.h>
#include <sys/block.h>
#include <sys/ioctl.h>

#define ATA_SIG 0x00000101
#define ATA_COMMAND_SETS_LBA48 (1U << 26)

typedef struct mmc_disk {
	block_device_t block_device;
} mmc_disk_t;

static void mmc_finish_callback(ioreq_t *ioreq, void *data) {
	block_request_t *request = data;
	// bubble up
	ioreq_finish(&request->ioreq, ioreq->ret);
}

static int mmc_submit(block_device_t *block_device, block_request_t *request) {
	scsi_device_t *device = container_of(block_device->device.devnode, mmc_device_t, devnode);
	mmc_disk_t *disk     = container_of(block_device, mmc_disk_t, block_device);

	// cannot write to a cdrom
	if (request->type != BLOCK_REQUEST_READ) return -EOPNOTSUPP;

	// TODO : verify we are in a data section

	scsi_command_t *command = scsi_create_read_command(device, request->start_sector, request->sectors_count, 0, request->buf, request->sectors_count * block_device->sector_size);
	if (!command) return -ENOMEM;

	ioreq_set_callback(&command->ioreq, mmc_finish_callback, request);
	return ioreq_submit(&command->ioreq);
}

static int mmc_ioctl(block_device_t *block_device, long req, void *arg) {
	if (device_is_unplugged(&block_device->device)) {
		return -ENXIO;
	}
	scsi_device_t *device = container_of(block_device->device.devnode, mmc_device_t, devnode);
	switch (req) {
	case DEVICE_GET_INFO:
		return safe_copy_auto_to(arg, &device->info);
	default:
		return -EINVAL;
	}
}

static void mmc_cleanup(block_device_t *block_device) {
	mmc_disk_t *disk = container_of(block_device, mmc_disk_t, block_device);
	kfree(disk);
}

static block_ops_t mmc_ops = {
	.submit  = mmc_submit,
	.ioctl   = mmc_ioctl,
	.cleanup = mmc_cleanup,
};

static int mmc_check(devnode_t *devnode) {
	scsi_device_t *device = container_of(devnode, scsi_device_t, devnode);
	return device->type == SCSI_INQUIRY_PERIPHERAL_TYPE_MMC5;
}

static int mmc_probe(devnode_t *devnode) {
	scsi_device_t *device = container_of(devnode, scsi_device_t, devnode);

	// send READ TOC and see size of the cdrom
	// TODO : support multi sessions disks
	scsi_read_toc_data_t read_toc_data = {0};
	ssci_read_toc_t read_toc_cmd = {
		.opcode = SCSI_READ_TOC_OPCODE,
		.format = SCSI_READ_TOC_FORMAT_FORMATTED_TOC,
		.allocation_length = scsi_uint16_to_data16(sizeof(read_toc_data)),
	};
	scsi_command_t *command = scsi_create_command(device, &read_toc_cmd, sizeof(read_toc_cmd));
	if (!command) return -ENOMEM;
	command->buf      = &read_toc_data;
	command->buf_size = sizeof(read_toc_data);

	int ret = ioreq_submit_sync(&command->ioreq);
	if (ret < 0) return ret;

	// default sector size of 2048
	size_t sector_size = 2048;
	size_t sectors_count = 0;

	for (size_t i = 0; i < SCSI_READ_TOC_MAX_TRACKS; i++) {
		size_t track_start = scsi_data32_to_uint32(&read_toc_data.formatted_toc[i].track_start);
		if (track_start > sectors_count) {
			sectors_count = track_start;
		}	
		if (read_toc_data.formatted_toc[i].track_number == SCSI_READ_TOC_LEAD_OUT) {
			// this is the lead out track
			break;
		}
	}

	// try read capacity (only supported on data disks)
	scsi_read_capacity10_data_t read_capacity_data = {0};
	scsi_read_capacity10_t read_capacity_cmd = {
		.opcode = SCSI_READ_CAPACITY10_OPCODE,
	};
	command = scsi_create_command(device, &read_capacity_cmd, sizeof(read_capacity_cmd));
	if (!command) return -ENOMEM;
	command->buf      = &read_capacity_data;
	command->buf_size = sizeof(read_capacity_data);


	ret = ioreq_submit_sync(&command->ioreq);
	if (ret >= 0) {
		// TODO : if max lba is 0xffffffff we need to try READ CAPACITY(16)
		// TODO : move read capacity stuff to libscsi
		// the drive support read capacity
		// we can get drive info from it
		sector_size   = scsi_data32_to_uint32(&read_capacity_data->block_length);
		sectors_count = scsi_data32_to_uint32(&read_capacity_data->max_lba);
	}

	mmc_disk_t *disk = kmalloc(sizeof(mmc_disk_t));
	if (!disk) return -ENOMEM;
	memset(disk, 0, sizeof(mmc_disk_t));
	disk->block_device.ops = &mmc_ops;
	disk->block_device.sector_size = sector_size;
	disk->block_device.sectors_count = sectors_count;
	disk->block_device.device.devnode = devnode;

	block_device_register(&disk->block_device, NULL, 0);
	return 0;
}

static void mmc_detach(devnode_t *devnode) {
	device_destroy(devnode->device);
}

static driver_t mmc_driver = {
	.name = "ATA disk",
	.device_name = "cdrom",
	.buses = "scsi_bus",
	.check  = mmc_check,
	.probe  = mmc_probe,
	.detach = mmc_detach,
};

static int mmc_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	return driver_register(&mmc_driver);
}

static int mmc_fini(void) {
	return driver_unregister(&mmc_driver);
}

kmodule_t module_meta = {
	.magic       = MODULE_MAGIC,
	.init        = mmc_init,
	.fini        = mmc_fini,
	.author      = "tayoky",
	.name        = "mmc",
	.description = "SCSI MMC disk driver",
	.license     = "GPL 3",
};
