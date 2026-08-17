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
	uint8_t status;
	for (size_t timeout = 0; timeout < 10000; timeout++) {
		status = ide_channel_read(channel, IDE_REG_STATUS);
		if (status & IDE_SR_ERR) {
			kwarningf("error %hhx\n", ide_channel_read(channel, IDE_REG_ERROR));
			return -EIO;
		}
		if ((status & mask) == value) return 0;
	}
	kwarningf("timeout expired status=%hhx mask=%hhx value=%hhx\n", status, mask, value);
	return -ETIMEDOUT;
}

static int ide_channel_reset(ide_channel_t *channel) {
	// soft reset
	mutex_acquire(&channel->mutex);
	ide_channel_write(channel, IDE_REG_CONTROL, 0x4 | channel->nIEN);
	ide_channel_io_wait(channel);
	ide_channel_write(channel, IDE_REG_CONTROL, channel->nIEN);
	ide_channel_io_wait(channel);

	int ret = ide_channel_poll(channel, IDE_SR_BSY, 0);
	mutex_release(&channel->mutex);
	return ret;
}

static ata_device_t *ide_channel_create_child(ide_channel_t *channel, devnode_t *bus, uint8_t drive) {
	// select the drive
	uint8_t drv_select = IDE_DRV_SELECT_LEGACY | IDE_DRV_SELECT_LBA | drive;
	ide_channel_write(channel, IDE_REG_DRV_SELECT, drv_select);

	ide_channel_io_wait(channel);
	uint8_t status = ide_channel_read(channel, IDE_REG_STATUS);
	if (status == 0 || status == 0xff) {
		// no drive
		return NULL;
	}
	if (ide_channel_poll(channel, IDE_SR_BSY, 0) < 0) {
		return NULL;
	}

	uint32_t signature = 
		((uint32_t)ide_channel_read(channel, IDE_REG_LBA2) << 24) |
		((uint32_t)ide_channel_read(channel, IDE_REG_LBA1) << 16) |
		((uint32_t)ide_channel_read(channel, IDE_REG_LBA0) << 8) |
		((uint32_t)ide_channel_read(channel, IDE_REG_SECCOUNT0) << 0);

	if (signature == 0x00000000 || signature == 0xffffffff) {
		// no device
		return NULL;
	}
	kdebugf("got signature %08x\n", signature);

	ata_device_t *device = kmalloc(sizeof(ata_device_t));
	if (!device) return NULL;
	memset(device, 0, sizeof(ata_device_t));
	device->channel = bus;
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
	int ret = 0;
	channel->base  = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, IDE_RID_BASE);
	channel->ctrl  = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, IDE_RID_CTRL);
	channel->bmide = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, IDE_RID_BMIDE);
	channel->nIEN = 0x2;
	if (IS_ERR(channel->base)) {
		ret = PTR2ERR(channel->base);
		goto error;
	}
	if (IS_ERR(channel->ctrl)) {
		ret = PTR2ERR(channel->ctrl);
		goto error;
	}
	
	ret = ide_channel_reset(channel);
	if (ret < 0) {
error:
		device_release_resource(devnode, channel->base);
		device_release_resource(devnode, channel->ctrl);
		device_release_resource(devnode, channel->bmide);
		return ret;
	}

	// create children ata channels
	channel->master = ide_channel_create_child(channel, devnode, 0);
	channel->slave = ide_channel_create_child(channel, devnode, IDE_DRV_SELECT_SLAVE);
	return 0;
}

static void ide_channel_detach(devnode_t *devnode) {
	ide_channel_t *channel = devnode->private;
	device_release_resource(devnode, channel->base);
	device_release_resource(devnode, channel->ctrl);
	device_release_resource(devnode, channel->bmide);
}

static int ide_channel_raw_send_ata_command(ide_channel_t *channel, ata_device_t *device, ata_command_t *command) {
	// select the drive
	// TODO : don't reselect if is was already selected
	uint8_t drv_select = IDE_DRV_SELECT_LEGACY | IDE_DRV_SELECT_LBA;
	if (device == channel->slave) {
		drv_select |= IDE_DRV_SELECT_SLAVE;
	}
	if (command->flags & ATA_CMD_SEND_LBA28) {
		drv_select |= (uint8_t)((command->lba >> 24) & 0xf);
	}
	ide_channel_write(channel, IDE_REG_DRV_SELECT, drv_select);

	kdebugf("send command opcode=%hhx sectors_count=%zu lba=%zu flags=%x\n", command->opcode, command->sectors_count, command->lba, command->flags);

	ide_channel_io_wait(channel);
	uint8_t status = ide_channel_read(channel, IDE_REG_STATUS);
	if (status == 0 || status == 0xff) {
		// no drive
		return -ENODEV;
	}
	int ret = ide_channel_poll(channel, IDE_SR_BSY, 0);
	if (ret < 0) return ret;

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
			ret = ide_channel_poll(channel, IDE_SR_BSY | IDE_SR_DRQ, IDE_SR_DRQ);
			if (ret < 0) return ret;
			for (size_t j = 0; j < 256; j++) {
				if (command->flags & ATA_CMD_WRITE_BUF) {
					resource_write16(channel->base, IDE_REG_DATA, *(buf++));
				} else {
					*(buf++) = resource_read16(channel->base, IDE_REG_DATA);
				}
			}
		}
	}
	
	return ide_channel_poll(channel, IDE_SR_BSY, 0);
}

// TODO : true async
static int ide_channel_submit_ata_command(devnode_t *bus, ata_device_t *device, ata_command_t *command) {
	ide_channel_t *channel = bus->private;
	if (mutex_try_acquire(&channel->mutex) < 0) {
		return -EAGAIN;
	}
	int ret = ide_channel_raw_send_ata_command(channel, device, command);
	mutex_release(&channel->mutex);
	ioreq_finish(&command->ioreq, ret);
	return 0;
}

ata_driver_t ide_channel_driver = {
	.driver = {
		.name = "IDE channel",
		.device_name = "ide_channel",
		.probe = ide_channel_probe,
		.detach = ide_channel_detach,
	},
	.submit_ata_command = ide_channel_submit_ata_command,
};
