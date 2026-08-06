#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/bus.h>
#include <module/ata.h>
#include <errno.h>
#include <ide.h>

// ide registers
#define IDE_REG_DATA       0x00
#define IDE_REG_ERROR      0x01
#define IDE_REG_FEATURES   0x01
#define IDE_REG_SECCOUNT0  0x02
#define IDE_REG_LBA0       0x03
#define IDE_REG_LBA1       0x04
#define IDE_REG_LBA2       0x05
#define IDE_REG_DRV_SELECT 0x06
#define IDE_REG_COMMAND    0x07
#define IDE_REG_STATUS     0x07
#define IDE_REG_SECCOUNT1  0x08
#define IDE_REG_LBA3       0x09
#define IDE_REG_LBA4       0x0A
#define IDE_REG_LBA5       0x0B
#define IDE_REG_CONTROL    0x0C
#define IDE_REG_ALTSTATUS  0x0C
#define IDE_REG_DEVADDRESS 0x0D

// ata status
#define IDE_SR_BSY  0x80 // busy
#define IDE_SR_DRDY 0x40 // drive ready
#define IDE_SR_DF   0x20 // drive write fault
#define IDE_SR_DSC  0x10 // drive seek complete
#define IDE_SR_DRQ  0x08 // data request ready
#define IDE_SR_CORR 0x04 // corrected data
#define IDE_SR_IDX  0x02 // index
#define IDE_SR_ERR  0x01 // error

#define IDE_DRV_SELECT_LEGACY 0xa0
#define IDE_DRV_SELECT_LBA    0x40
#define IDE_DRV_SELECT_SLAVE  0x10

static void ide_channel_write(ide_channel_t *channel, uint32_t reg, uint8_t data) {
	// set HOB
	if (reg > 0x07 && reg < 0x0C) {
		ide_channel_write(channel, IDE_REG_CONTROL, 0x80 | channel->nIEN);
	}
	if (reg <= IDE_REG_STATUS) {
		resource_write8(channel->base, reg, data);
	} else if (reg <= IDE_REG_LBA5) {
		resource_write8(channel->base, reg - 0x06, data);
	} else if (reg <= IDE_REG_DEVADDRESS) {
		resource_write8(channel->ctrl, reg - 0x0A, data);
	} else {
		resource_write8(channel->bmide, reg - 0xE, data);
	}

	if (reg > 0x07 && reg < 0x0C) {
		ide_channel_write(channel, IDE_REG_CONTROL, channel->nIEN);
	}
}

static uint8_t ide_channel_read(ide_channel_t *channel, uint32_t reg) {
	if (reg > 0x07 && reg < 0x0C) {
		ide_channel_write(channel, IDE_REG_CONTROL, 0x80 | channel->nIEN);
	}
	uint8_t data;
	if (reg <= IDE_REG_STATUS) {
		data = resource_read8(channel->base, reg);
	} else if (reg <= IDE_REG_LBA5) {
		data = resource_read8(channel->base, reg - 0x06);
	} else if (reg <= IDE_REG_DEVADDRESS) {
		data = resource_read8(channel->ctrl, reg - 0x0A);
	} else {
		data = resource_read8(channel->bmide, reg - 0xE);
	}
	if (reg > 0x07 && reg < 0x0C) {
		ide_channel_write(channel, IDE_REG_CONTROL, channel->nIEN);
	}
	return data;
}

static void ide_channel_io_wait(ide_channel_t *channel) {
	for (size_t i = 0; i < 4; i++) {
		ide_channel_read(channel, IDE_REG_ALTSTATUS);
	}
}

static int ide_channel_poll(ide_channel_t *channel, uint8_t mask, uint8_t value) {
	size_t timeout = 10000;
	while ((ide_channel_read(channel, IDE_REG_STATUS) & mask) != value) {
		if (--timeout <= 0) {
			kwarningf("timeout expired\n");
			return -ETIMEDOUT;
		};
	}
	return 0;
}

static void ide_channel_reset(ide_channel_t *channel) {
	// soft reset
	mutex_acquire(&channel->mutex);
	ide_channel_write(channel, IDE_REG_CONTROL, 0x4 | channel->nIEN);
	ide_channel_io_wait(channel);
	ide_channel_write(channel, IDE_REG_CONTROL, channel->nIEN);

	if (ide_channel_poll(channel, IDE_SR_BSY, 0) < 0) {
		return -ETIMEDOUT;
	}
	mutex_release(&channel->mutex);
}

static ata_device_t *ide_channel_create_child(ide_channel_t *channel, devnode_t *bus, uint8_t drive) {
	// select the drive
	uint8_t drv_select = IDE_DRV_SELECT_LEGACY | IDE_DRV_SELECT_LBA | drive;
	ide_channel_write(channel, IDE_REG_DEVSELECT, drv_select | drive);
	ide_channel_io_wait(channel);

	uint32_t signature = 
		(ide_channel_read(channel, IDE_REG_LBA2) << 24) |
		(ide_channel_read(channel, IDE_REG_LBA1) << 16) |
		(ide_channel_read(channel, IDE_REG_LBA0) << 8) |
		(ide_channel_read(channel, IDE_REG_SECCOUNT) << 0);

	if (signature == 0x00000000 || signature == 0xffffffff) {
		// no device
		return NULL;
	}
	kdebugf("got signature %032x\n", signature);

	ata_device_t *device = kmalloc(sizeof(ata_device_t));
	if (!device) return NULL;
	memset(device, 0, sizeof(ata_device_t));
	device->signature = signature;

	bus_attach_child(bus, &device->devnode, NULL, UNIT_NOUNIT);
	return device;
}

static int ide_channel_probe(devnode_t *devnode) {
	ide_channel_t *channel = kmalloc(sizeof(ide_channel_t));
	if (!channel) return -ENOMEM;
	memset(channel, 0, sizeof(ide_channel_t));
	mutex_init(&channel->mutex);
	devnode->private = channel;

	// get resources from the IDE controller
	channel->base  = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, IDE_RID_BASE);
	channel->ctrl  = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, IDE_RID_CTRL);
	channel->bmide = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, IDE_RID_BMIDE);
	channel->nIEN = 0x2;
	
	mutex_acquire(&channel->mutex);
	ide_channel_reset(channel);
	mutex_release(&channel->mutex);

	// create children ata channels
	channel->master = ide_channel_create_child(channel, devnode, 0, &channel->master_signature);
	channel->slave = ide_channel_create_child(channel, devnode, IDE_DRV_SELECT_SLAVE, &channel->slave_signature);
	return 0;
}

static void ide_channel_detach(devnode_t *devnode) {
	ide_channel_t *channel = devnode->private;
	device_release_resource(devnode, channel->base);
	device_release_resource(devnode, channel->ctrl);
	device_release_resource(devnode, channel->bmide);
}

static int ide_channel_raw_send_ata_command(ide_channel_t *channel, devnode_t *devnode, ata_command_t *command) {
	// select the drive
	// TODO : don't reselect if is was already selected
	uint8_t drv_select = IDE_DRV_SELECT_LEGACY | IDE_DRV_SELECT_LBA;
	if (devnode == channel->slave) {
		drv_select |= IDE_DRV_SELECT_SLAVE;
	}
	if (command->flags & ATA_CMD_SEND_LBA28) {
		drv_select |= (uint8_t)((command->lba >> 24) & 0xf);
	}
	ide_channel_write(channel, IDE_REG_DRV_SELECT, drv_select);

	ide_channel_io_wait(channel);
	uint8_t status = ide_channel_read(channel, IDE_REG_STATUS);
	if (status == 0 || status == 0xff) {
		// no drive
		return -ENODEV;
	}
	if (ide_channel_poll(channel, IDE_SR_BSY, 0) < 0) {
		return -ETIMEDOUT;
	}

	if (command->flags & ATA_CMD_SEND_LBA48) {
		ide_channel_write(channel, IDE_REG_SECCOUNT0, (uint8_t)(command->sectors_count >> 8));
		ide_channel_write(channel, IDE_REG_LBA0, (uint8_t)(command->lba >> 24));
		ide_channel_write(channel, IDE_REG_LBA1, (uint8_t)(command->lba >> 32));
		ide_channel_write(channel, IDE_REG_LBA2, (uint8_t)(command->lba >> 40));
	}

	if (command->flags & (ATA_CMD_SEND_LBA28 | ATA_CMD_SEND_LBA48)) {
		ide_channel_write(channel, IDE_REG_SECCOUNT0, (uint8_t)(command->sectors_count));
		ide_channel_write(channel, IDE_REG_LBA0, (uint8_t)(command->lba));
		ide_channel_write(channel, IDE_REG_LBA1, (uint8_t)(command->lba >> 8));
		ide_channel_write(channel, IDE_REG_LBA2, (uint8_t)(command->lba >> 16));
	}

	ide_channel_write(channel, IDE_REG_COMMAND, command->opcode);

	// TODO : DMA support
	uint16_t *buf = command->buf;
	if (command->flags & (ATA_CMD_READ_BUF | ATA_CMD_WRITE_BUF)) {
		for (size_t i = 0; i < command->sectors_count; i++) {
			ide_channel_io_wait(channel);
			if (ide_channel_poll(channel, IDE_SR_BSY | IDE_SR_DRQ, IDE_SR_DRQ) < 0) {
				return -EIO;
			}
			for (size_t j = 0; j < 256; j++) {
				if (command->flags & ATA_CMD_WRITE_BUF) {
					resource_write16(channel->base, IDE_REG_DATA, *(buf++));
				} else {
					*(buf++) = resource_read16(channel->base, IDE_REG_DATA);
				}
			}
		}
	} else {
			if (ide_channel_poll(channel, IDE_SR_BSY, 0) < 0) {
				return -EIO;
			}
	}
	if (ide_channel_read(channel, IDE_REG_STATUS) & IDE_SR_ERR) {
		return -EIO;
	}
	return 0;
}

static int ide_channel_send_ata_command(devnode_t *bus, devnode_t *devnode, ata_command_t *command) {
	ide_channel_t *channel = bus->private;
	mutex_acquire(&channel->mutex);
	int ret = ide_channel_raw_send_ata_command(channel, devnode, command);
	mutex_release(&channel->mutex);
	return ret;
}

ata_driver_t ide_channel_driver = {
	.driver = {
		.name = "IDE channel",
		.device_name = "ide_channel",
		.probe = ide_channel_probe,
		.detach = ide_channel_detach,
	},
	.send_ata_command = ide_channel_send_ata_command,
};
