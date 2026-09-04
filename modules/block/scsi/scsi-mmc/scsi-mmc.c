#include <kernel/userspace.h>
#include <kernel/module.h>
#include <kernel/block.h>
#include <kernel/kheap.h>
#include <kernel/bus.h>
#include <module/scsi.h>
#include <sys/block.h>
#include <sys/cdrom.h>

#define ATA_SIG 0x00000101
#define ATA_COMMAND_SETS_LBA48 (1U << 26)

typedef struct mmc_disk {
	block_device_t *block_device;
	cdrom_toc_header_t toc;
	size_t tracks_count;
	cdrom_toc_entry_t *tracks;
} mmc_disk_t;

static cdrom_toc_entry_t *mmc_get_track(mmc_disk_t *disk, size_t track) {
	for (size_t i = 0; i < disk->tracks_count; i++) {
		if (disk->tracks[i].track == track) {
			return &disk->tracks[i];
		}
	}
	return NULL;
}

static cdrom_toc_entry_t *mmc_get_track_at(mmc_disk_t *disk, size_t lba) {
	for (size_t i = 0; i < disk->tracks_count; i++) {
		uintptr_t start = disk->tracks[i].start;
		uintptr_t end   = disk->tracks[i].start + disk->tracks[i].size;
		if (start <= lba && end > lba) {
			return &disk->tracks[i];
		}
	}
	return NULL;
}

static void mmc_finish_callback(ioreq_t *ioreq, void *data) {
	block_request_t *request = data;
	// bubble up
	ioreq_finish(&request->ioreq, ioreq->ret);
}

static int mmc_submit(block_device_t *block_device, block_request_t *request) {
	scsi_device_t *device = container_of(block_device->device.devnode, scsi_device_t, devnode);
	mmc_disk_t *disk      = block_device->private;

	// cannot write to a cdrom
	if (request->type != BLOCK_REQUEST_READ) return -EOPNOTSUPP;

	cdrom_toc_entry_t *start_track = mmc_get_track_at(disk, request->start_sector);
	cdrom_toc_entry_t *end_track   = mmc_get_track_at(disk, request->start_sector + request->sectors_count);

	// we cannot cross a track boundary
	if (start_track != end_track) {
		return -EINVAL;
	}

	// we cannot read outside a data track
	if (!start_track || !(start_track->flags & SCSI_READ_TOC_CONTROL_DATA)) {
		return -EINVAL;
	}

	scsi_command_t *command = scsi_create_read_command(device, request->start_sector, request->sectors_count, 0); 
	if (!command) return -ENOMEM;

	int ret = iobuf_dup(&command->iobuf, &request->iobuf);
	if (ret < 0) {
		ioreq_release(&command->ioreq);
		return ret;
	}

	ioreq_set_callback(&command->ioreq, mmc_finish_callback, request);
	return ioreq_submit(&command->ioreq);
}

static int mmc_ioctl(block_device_t *block_device, long req, void *arg) {
	if (block_device_is_unplugged(block_device)) {
		return -ENXIO;
	}
	scsi_device_t *device = container_of(block_device->device.devnode, scsi_device_t, devnode);
	mmc_disk_t *disk      = block_device->private;
	switch (req) {
	case DEVICE_GET_INFO:
		return safe_copy_auto_to(arg, &device->info);
	case CDROM_EJECT:
	case CDROM_LOCK:
	case CDROM_UNLOCK:
		// TODO
		return -ENOSYS;
	case CDROM_READ_TOC_HEADER:
		return safe_copy_auto_to(arg, &disk->toc);
	case CDROM_READ_TOC_ENTRY:;
		cdrom_toc_entry_t current;
		if (safe_copy_auto_from(&current, arg) < 0) return -EFAULT;
		cdrom_toc_entry_t *track = mmc_get_track(disk, current.track);
		if (!track) return -ENOENT;
		return safe_copy_auto_to(arg, track);
	default:
		return -ENOTTY;
	}
}

static block_ops_t mmc_ops = {
	.submit  = mmc_submit,
	.ioctl   = mmc_ioctl,
};

static int mmc_check(devnode_t *devnode) {
	scsi_device_t *device = container_of(devnode, scsi_device_t, devnode);
	return device->type == SCSI_INQUIRY_PERIPHERAL_TYPE_MMC5;
}

static int mmc_probe(devnode_t *devnode) {
	scsi_device_t *device = container_of(devnode, scsi_device_t, devnode);
	mmc_disk_t *disk = devnode->private;

	// send READ TOC and see size of the cdrom
	// TODO : support multi sessions disks
	scsi_read_toc_data_t read_toc_data = {0};
	scsi_read_toc_t read_toc_cmd = {
		.opcode = SCSI_READ_TOC_OPCODE,
		.format = SCSI_READ_TOC_FORMAT_FORMATED_TOC,
		.allocation_length = scsi_uint16_to_data16(sizeof(read_toc_data)),
	};
	scsi_command_t *command = scsi_create_command(device, &read_toc_cmd, sizeof(read_toc_cmd));
	if (!command) return -ENOMEM;
	iobuf_init_continuous(&command->iobuf, &read_toc_data, sizeof(read_toc_data));

	int ret = ioreq_submit_sync(&command->ioreq);
	if (ret < 0) return ret;

	// default sector size of 2048
	size_t sector_size = 2048;
	size_t sectors_count = 0;

	size_t tracks_count = 0;
	for (size_t i = 0; i < SCSI_READ_TOC_MAX_TRACKS; i++) {
		size_t track_start = scsi_data32_to_uint32(&read_toc_data.formatted_toc[i].track_start);
		if (track_start > sectors_count) {
			sectors_count = track_start;
		}	
		if (read_toc_data.formatted_toc[i].track_number == SCSI_READ_TOC_LEAD_OUT) {
			// this is the lead out track
			break;
		}
		tracks_count++;
	}

	if (tracks_count >= SCSI_READ_TOC_MAX_TRACKS) {
		kwarningf("too many tracks\n");
		return -ENOTSUP;
	}

	// try read capacity (only supported on data disks)
	scsi_read_capacity10_data_t read_capacity_data = {0};
	scsi_read_capacity10_t read_capacity_cmd = {
		.opcode = SCSI_READ_CAPACITY10_OPCODE,
	};
	command = scsi_create_command(device, &read_capacity_cmd, sizeof(read_capacity_cmd));
	if (!command) return -ENOMEM;
	iobuf_init_continuous(&command->iobuf, &read_capacity_data, sizeof(read_capacity_data));

	ret = ioreq_submit_sync(&command->ioreq);
	if (ret >= 0) {
		// TODO : if max lba is 0xffffffff we need to try READ CAPACITY(16)
		// TODO : move read capacity stuff to libscsi
		// the drive support read capacity
		// we can get drive info from it
		sector_size   = scsi_data32_to_uint32(&read_capacity_data.block_length);
		sectors_count = scsi_data32_to_uint32(&read_capacity_data.max_lba);
	}

	disk->block_device = block_device_allocate();
	if (!disk->block_device) return -ENOMEM;
	disk->block_device->ops = &mmc_ops;
	disk->block_device->sector_size    = sector_size;
	disk->block_device->sectors_count  = sectors_count;
	disk->block_device->device.devnode = devnode;
	disk->block_device->private        = disk;
	disk->toc.first_track = read_toc_data.first_track;
	disk->toc.last_track  = read_toc_data.last_track;
	disk->tracks_count = tracks_count;
	disk->tracks = kmalloc(sizeof(cdrom_toc_entry_t) * tracks_count);
	if (!disk->tracks) {
		block_device_release(disk->block_device);
		return -ENOMEM;
	}
	memset(disk->tracks, 0, sizeof(cdrom_toc_entry_t) * tracks_count);
	for (size_t i = 0; i < tracks_count; i++) {
		size_t track_start = scsi_data32_to_uint32(&read_toc_data.formatted_toc[i].track_start);
		size_t track_end = scsi_data32_to_uint32(&read_toc_data.formatted_toc[i + 1].track_start);
		size_t track_number = read_toc_data.formatted_toc[i].track_number;
		size_t flags = read_toc_data.formatted_toc[i].flags;

		disk->tracks[i].track = track_number;
		disk->tracks[i].start = track_start;
		disk->tracks[i].size  = track_end - track_start;
		disk->tracks[i].flags = flags;
	}

	block_device_register(disk->block_device, NULL, 0);
	return 0;
}

static void mmc_detach(devnode_t *devnode) {
	mmc_disk_t *disk = devnode->private;
	block_device_destroy(disk->block_device);
}

static driver_t mmc_driver = {
	.name = "ATA disk",
	.device_name = "cd",
	.buses = BUSES("scsi_bus"),
	.private_size = sizeof(mmc_disk_t),
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
