#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/bus.h>
#include <kernel/time.h>
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
	uint8_t status = 0;
	struct timespec current;
	gettime(CLOCK_MONOTONIC, &current);
	struct timespec timeout = current;
	timeout.tv_nsec += 500000000;
	if (timeout.tv_nsec >= 1000000000) {
		timeout.tv_nsec -= 1000000000;
		timeout.tv_sec++;
	}
	while (timespec_cmp(&current, &timeout) <= 0) {
		gettime(CLOCK_MONOTONIC, &current);
		status = ide_channel_read(channel, IDE_REG_STATUS);
		if ((status & mask) == value) break;
		if (status & IDE_SR_ERR) break;
		yield(1);
	}
	if (status & IDE_SR_ERR) {
		kwarningf("error status=%hhx error=%hhx\n", status, ide_channel_read(channel, IDE_REG_ERROR));
		return -EIO;
	}
	if ((status & mask) == value) return 0;
	kwarningf("timeout expired status=%hhx mask=%hhx value=%hhx\n", status, mask, value);
	return -ETIMEDOUT;
}

static void ide_channel_enable_irq(ide_channel_t *channel) {
	channel->nIEN &= ~0x2U;
	ide_channel_write(channel, IDE_REG_CONTROL, channel->nIEN);
}

static void ide_channel_disable_irq(ide_channel_t *channel) {
	channel->nIEN |= 0x2;
	ide_channel_write(channel, IDE_REG_CONTROL, channel->nIEN);
}

static void ide_channel_transfer_sector(ide_channel_t *channel, uint16_t *buf, long flags) {
	for (size_t i = 0; i < 256; i++) {
		if (flags & ATA_CMD_WRITE_BUF) {
			resource_write16(channel->base, IDE_REG_DATA, buf[i]);
		} else if (flags & ATA_CMD_READ_BUF) {
			buf[i] = resource_read16(channel->base, IDE_REG_DATA);
		}
	}
}

static void ide_channel_irq_handler(registers_t *registers, void *data) {
	(void)registers;
	ide_channel_t *channel = data;
	ata_command_t *command = atomic_load(&channel->current_command);

	ide_channel_io_wait(channel);
	uint8_t status = ide_channel_read(channel, IDE_REG_STATUS);
	if (status & IDE_SR_ERR) {
		kwarningf("error status=%hhx error=%hhx\n", status, ide_channel_read(channel, IDE_REG_ERROR));
error:
		channel->ret = -EIO;
		work_queue(&channel->work);
		return;
	}

	if (status & IDE_SR_BSY) {
		// surpirous wakeup
		return;
	}

	// do we have sectors left
	if (channel->current_sector < command->sectors_count) {
		if (!(status & IDE_SR_DRQ)) {
			kwarningf("expected data request status=%hhx\n", status);
			goto error;
		}
		uint16_t *buf = command->buf;
		ide_channel_transfer_sector(channel, buf + (channel->current_sector++) * 256, command->flags);
		if (channel->current_sector < command->sectors_count) {
			// we have others sectors to read/write
			return;
		} else if (command->flags & ATA_CMD_WRITE_BUF) {
			// we will get another irq for confirmation
			// of last sector write
			return;
		}
	} else {
		if (status & IDE_SR_DRQ) {
			kwarningf("unexpected data request status=%hhx\n", status);
		}
	}
	
	// command finished :D
	channel->ret = 0;
	work_queue(&channel->work);
}

/**
 * @brief triggered when a command finish
 * @param work the work of the ide channel
 */
static void ide_channel_work(work_t *work) {
	ide_channel_t *channel = container_of(work, ide_channel_t, work);
	ata_command_t *command = channel->current_command;

	atomic_store(&channel->current_command, NULL);
	ioreq_finish(&command->ioreq, channel->ret);
}

static int ide_channel_reset(ide_channel_t *channel) {
	// soft reset
	ide_channel_write(channel, IDE_REG_CONTROL, 0x4 | channel->nIEN);
	ide_channel_io_wait(channel);
	ide_channel_write(channel, IDE_REG_CONTROL, channel->nIEN);
	ide_channel_io_wait(channel);

	return ide_channel_poll(channel, IDE_SR_BSY, 0);
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
	devnode->private = channel;

	// get resources from the IDE controller
	int ret = 0;
	channel->base  = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, IDE_RID_BASE);
	channel->ctrl  = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, IDE_RID_CTRL);
	channel->bmide = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, IDE_RID_BMIDE);
	if (!disable_irq) {
		channel->irq = device_allocate_simple_resource(devnode, RESOURCE_IRQ, IDE_RID_IRQ);
	}
	channel->nIEN = 0x2;
	if (IS_ERR(channel->base)) {
		ret = PTR2ERR(channel->base);
		goto error;
	}
	if (IS_ERR(channel->ctrl)) {
		ret = PTR2ERR(channel->ctrl);
		goto error;
	}
	
	if (channel->irq && !IS_ERR(channel->irq)) {
		work_init(&channel->work, ide_channel_work);
		channel->irq_handler = resource_register_handler(channel->irq, ide_channel_irq_handler, channel);
	}
	
	ret = ide_channel_reset(channel);
	if (ret < 0) {
error:
		resource_unregister_handler(channel->irq, channel->irq_handler);
		device_release_resource(devnode, channel->base);
		device_release_resource(devnode, channel->ctrl);
		device_release_resource(devnode, channel->bmide);
		device_release_resource(devnode, channel->irq);
		return ret;
	}

	// create children ata channels
	channel->master = ide_channel_create_child(channel, devnode, 0);
	channel->slave = ide_channel_create_child(channel, devnode, IDE_DRV_SELECT_SLAVE);

	if (channel->irq_handler) {
		ide_channel_enable_irq(channel);
	}
	return 0;
}

static void ide_channel_detach(devnode_t *devnode) {
	ide_channel_t *channel = devnode->private;
	resource_unregister_handler(channel->irq, channel->irq_handler);
	device_release_resource(devnode, channel->base);
	device_release_resource(devnode, channel->ctrl);
	device_release_resource(devnode, channel->bmide);
	device_release_resource(devnode, channel->irq);
}

static int ide_channel_poll_mode(ide_channel_t *channel, ata_command_t *command) {
	// TODO : DMA support
	uint16_t *buf = command->buf;
	while (channel->current_sector < command->sectors_count) {
		ide_channel_io_wait(channel);
		int ret = ide_channel_poll(channel, IDE_SR_BSY | IDE_SR_DRQ, IDE_SR_DRQ);
		if (ret < 0) return ret;
		ide_channel_transfer_sector(channel, buf + (channel->current_sector++ * 256), command->flags);
	}
	
	ide_channel_io_wait(channel);
	int ret = ide_channel_poll(channel, IDE_SR_BSY, 0);
	if (ret < 0) return ret;

	atomic_store(&channel->current_command, NULL);
	ioreq_finish(&command->ioreq, 0);
	return 0;
}

static int ide_channel_raw_send_ata_command(ide_channel_t *channel, ata_device_t *device, ata_command_t *command) {
	int ret = ide_channel_poll(channel, IDE_SR_BSY, 0);
	if (ret < 0) return ret;

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
	ret = ide_channel_poll(channel, IDE_SR_BSY, 0);
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
	channel->current_sector = 0;

	if ((command->flags & ATA_CMD_WRITE_BUF) && command->sectors_count > 0) {
		// we need to write the first sector
		ide_channel_io_wait(channel);
		int ret = ide_channel_poll(channel, IDE_SR_DRQ, IDE_SR_DRQ);
		if (ret < 0) return ret;
		ide_channel_transfer_sector(channel, command->buf, command->flags);
		channel->current_sector++;
	}

	if (channel->irq_handler) {
		// the irq handler will take care of the rest
		return 0;
	} else {
		return ide_channel_poll_mode(channel, command);
	}
}

static int ide_channel_submit_ata_command(devnode_t *bus, ata_device_t *device, ata_command_t *command) {
	ide_channel_t *channel = bus->private;
	ata_command_t *expected = NULL;
	if (!atomic_compare_exchange_strong(&channel->current_command, &expected, command)) {
		return -EAGAIN;
	}
	int ret = ide_channel_raw_send_ata_command(channel, device, command);
	if (ret < 0) {
		atomic_store(&channel->current_command, NULL);
	}
	return ret;
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
