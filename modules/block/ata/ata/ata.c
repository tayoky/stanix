#include <kernel/module.h>
#include <kernel/device.h>
#include <kernel/bus.h>
#include <module/ata.h>
#include <sys/block.h>

// thanks Sasdallas for this
typedef struct ata_ident {
	uint16_t flags;           // If bit 15 is cleared, valid drive. If bit 7 is set to one, this is removable.
	uint16_t obsolete;        // Obsolete
	uint16_t specifics;       // 7.17.7.3 in specification
	uint16_t obsolete2[6];    // Obsolete
	uint16_t obsolete3;       // Obsolete
	char serial[20];          // Serial number
	uint16_t obsolete4[3];    // Obsolete
	char firmware[8];         // Firmware revision
	char model[40];           // Model number
	uint16_t rw_multiple;     // R/W multiple support (<=16 is SATA)
	uint16_t obsolete5;       // Obsolete
	uint32_t capabilities;    // Capabilities of the IDE device
	uint16_t obsolete6[2];    // Obsolete
	uint16_t field_validity;  // If 1, the values reported in _ - _ are valid
	uint16_t obsolete7[5];    // Obsolete
	uint16_t multi_sector;    // Multiple sector setting
	uint32_t sectors;         // Total addressable sectors
	uint16_t obsolete8[20];   // Technically these aren't obsolete, but they contain nothing really useful
	uint32_t command_sets;    // Command/feature sets
	uint16_t obsolete9[16];   // Contain nothing really useful
	uint64_t sectors_lba48;   // LBA48 maximum sectors, AND by 0000FFFFFFFFFFFF for validity
	uint16_t obsolete10[152]; // Contain nothing really useful
} __attribute__((packed)) __attribute__((aligned(8))) ata_ident_t;

static ssize_t ata_access(vfs_fd_t *fd, void *buffer, size_t offset, size_t count, int write) {
	ata_device_t *device = fd->private;
	if (offset >= device->size) {
		return 0;
	}
	if (offset + count >= device->size) {
		count = device->size - offset;
	}

	uint64_t lba         = offset / 512;
	uint64_t end         = offset + count;
	size_t sectors_count = (end + 511) / 512 - lba;
	if (!sectors_count) return 0;

	// LBA28 has a lower limit
	if (lba >= 0x10000000 && !(device->command_sets & (1 << 26))) {
		// high LBA but no LBA28 support... uh ?
		return -EIO;
	}

	void *buf = kmalloc(sectors_count * 512);

	// for write we need to fill first and last sectors
	if (write) {
		if (offset % 512) {
			ata_access(fd, buf, lba * 512, 512, 0);
		}
		if (sectors_count > 1 && end % 512) {
			ata_access(fd, &buf[256 * (sectors_count - 1)], (lba + sectors_count - 1) * 512, 512, 0);
		}
	}

	uint8_t opcode;
	if (device->command_sets & (1 << 26)) {
		if (write) {
			opcode = ATA_CMD_WRITE_PIO_EXT;
		} else {
			opcode = ATA_CMD_READ_PIO_EXT;
		}
	} else {
		if (write) {
			opcode = ATA_CMD_WRITE_PIO;
		} else {
			opcode = ATA_CMD_READ_PIO;
		}
	}

	ata_command_t command = {
		.opcode = opcode,
		.sectors_count = sectors_count,
		.lba = lba,
		.flags = (device->command_sets & (1 << 26)) ? ATA_CMD_SEND_LBA48 : ATA_CMD_SEND_LBA_28,
		.buf   = buf,
	}
	int ret = ata_send_command(devnode, &command);
	if (ret < 0) {
		kfree(buf);
		return ret;
	}

	// we need to send cache flush on write
	if (write) {
		ata_command_t flush_command = {
			.opcode = (device->command_sets & (1 << 26)) ? ATA_CMD_FLUSH : ATA_CMD_FLUSH_EXT,
		};
		ata_send_command(devnode, &command);
	}

	memcpy(buffer, ((char *)buf) + offset % 512, count);
	kfree(buf);

	return count;
}
static ssize_t ata_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	return ata_access(fd, buffer, offset, count, 0);
}

static ssize_t ata_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	return ata_access(fd, (void *)buffer, offset, count, 1);
}

static int ata_ioctl(vfs_fd_t *fd, long req, void *arg) {
	ata_device_t *device = fd->private;
	switch (req) {
	case I_BLOCK_GET_SIZE:
		*(size_t *)arg = device->size;
		return 0;
	case I_MODEL:
		strcpy(arg, device->model);
		return 0;
	default:
		return -EINVAL;
	}
}

static vfs_ops_t ata_disk_ops = {
	.read  = ata_read,
	.write = ata_write,
	.ioctl = ata_ioctl,
};

static int ata_check(devnode_t *devnode) {
	// TODO : check here, but what ?
	return 1;
}

static int ata_probe(devnode_t *devnode) {
	ata_ident_t ident,
	ata_command_t identify = {
		.opcode = ATA_CMD_IDENTIFY,
		.lba = 0,
		.sectors_count = 0,
		.flags = ATA_CMD_SEND_LBA28,
		.buf = &ident,
	}
	int ret = ata_send_command(devnodd, &identify);
	if (ret < 0) return ret;


	ata_device_t *device;
	uint64_t sectors = ident.command_sets & (1 << 26) ? ident.sectors_lba48 : ident.sectors;
	for (size_t i = 0; i < sizeof(ident.model); i += 2) {
		device->model[i + 1] = ident.model[i];
		device->model[i]     = ident.model[i + 1];
	}
	for (size_t i = sizeof(device->model) - 1; i > 0 && device->model[i] == ' '; i--) {
		device->model[i] = '\0';
	}
	device->command_sets = ident.command_sets;

	kdebugf("model : %s command sets : %x support LBA48 : %s max LBA : %ld\n", device->model, ident.command_sets, ident.command_sets & (1 << 26) ? "true" : "false", sectors);
	return 0;
}

static driver_t ata_driver = {
	.name = "ATA disk",
	.device_name = "hd%c",
	.buses = BUSES("ide_channel%d"),
	.check = ata_check,
	.probe = ata_probe,
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
