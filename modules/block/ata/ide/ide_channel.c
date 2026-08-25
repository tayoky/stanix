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
		if (status & ATA_SR_ERR) break;
		yield(1);
	}
	if (status & ATA_SR_ERR) {
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

static void ide_channel_send_data(ide_channel_t *channel, const uint16_t *buf, size_t count) {
	kassert(count % 2 == 0);
	for (size_t i = 0; i < count; i += 2) {
		resource_write16(channel->base, IDE_REG_DATA, buf[i]);
	}
}

static void ide_channel_receive_data(ide_channel_t *channel, uint16_t *buf, size_t count) {
	kassert(count % 2 == 0);
	for (size_t i = 0; i < count; i += 2) {
		buf[i] = resource_read16(channel->base, IDE_REG_DATA);
	}
}

static size_t ide_channel_get_transfer_size(ide_channel_t *channel, ata_command_t *command) {
	if (command->flags & ATA_CMD_PACKET_PROTOCOL) {
		uint8_t lba1 = ide_channel_read(channel, IDE_REG_LBA1);
		uint8_t lba2 = ide_channel_read(channel, IDE_REG_LBA2);
		return ((uint16_t)lba2 << 8) | lba1;
	} else {
		// FIXME : some commands could use a different transfer_size
		return 512;
	}
}

static int ide_channel_transfer(ide_channel_t *channel, ata_command_t *command) {
	size_t transfer_size = ide_channel_get_transfer_size(channel, command);
	if (channel->bytes_transferred + transfer_size > command->buf_size) {
		// more data than expected ?
		kwarning("more data than expected\n");
		return -EIO;
	}

	uint16_t *buf = command->buf;
	buf += channel->bytes_transferred / sizeof(uint16_t);
	
	channel->bytes_transferred += transfer_size;
	if (command->flags & ATA_CMD_WRITE_BUF) {
		ide_channel_send_data(channel, buf, transfer_size);
	} else {
		ide_channel_receive_data(channel, buf, transfer_size);
	}
	return 0;
}

static void ide_channel_send_packet(ide_channel_t *channel, ata_command_t *command) {
	uint16_t packet[8];
	memcpy(packet, command->packet, sizeof(packet));
	kassert(command->packet_length < sizeof(packet));
	ide_channel_send_data(channel, packet, command->packet_length);
}

static void ide_channel_irq_handler(registers_t *registers, void *data) {
	(void)registers;
	ide_channel_t *channel = data;
	ata_command_t *command = atomic_load(&channel->current_command);

	ide_channel_io_wait(channel);
	uint8_t status = ide_channel_read(channel, IDE_REG_STATUS);

	int ret = 0;
	if (status & ATA_SR_ERR) {
		kwarningf("error status=%hhx error=%hhx\n", status, ide_channel_read(channel, IDE_REG_ERROR));
		ret = -EIO;
error:
		channel->ret = ret;
		work_queue(&channel->work);
		return;
	}

	if (status & ATA_SR_BSY) {
		// spurious wakeup
		return;
	}

	if (channel->bytes_transferred < command->buf_size) {
		// do we have remaining data to transfer
		if (!(status & ATA_SR_DRQ)) {
			kwarningf("expected data request status=%hhx\n", status);
			ret = -EIO;
			goto error;
		}

		ret = ide_channel_transfer(channel, command);
		if (ret < 0) goto error;

		if (channel->bytes_transferred < command->buf_size) {
			// we have others transfer to send/receive
			return;
		} else if (command->flags & ATA_CMD_WRITE_BUF) {
			// we will get another irq for confirmation
			// of last transfer write
			return;
		}
	} else {
		if (status & ATA_SR_DRQ) {
			kwarningf("unexpected data request status=%hhx\n", status);
			ret = -EIO;
			goto error;
		}
	}
	
	// command finished :D
	channel->ret = 0;
	work_queue(&channel->work);
	return 0;
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

	return ide_channel_poll(channel, ATA_SR_BSY, 0);
}

static ata_device_t *ide_channel_create_child(ide_channel_t *channel, devnode_t *bus, uint8_t drive) {
	// select the drive
	uint8_t drv_select = ATA_DRV_SELECT_LEGACY | ATA_DRV_SELECT_LBA | drive;
	ide_channel_write(channel, IDE_REG_DRV_SELECT, drv_select);

	ide_channel_io_wait(channel);
	uint8_t status = ide_channel_read(channel, IDE_REG_STATUS);
	if (status == 0 || status == 0xff) {
		// no drive
		return NULL;
	}
	if (ide_channel_poll(channel, ATA_SR_BSY, 0) < 0) {
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
	channel->slave = ide_channel_create_child(channel, devnode, ATA_DRV_SELECT_SLAVE);

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

static int ide_channel_irq_mode(ide_channel_t *channel, ata_command_t *command) {
	if (command->flags & ATA_CMD_PACKET_PROTOCOL) {
		// for packet protocol the irq will trigger when ready to transfer data
		return 0;
	}

	if ((command->flags & ATA_CMD_WRITE_BUF) && command->buf_size > 0) {
		// for non packet protocol we need to write the first transfer since it does not trigger an irq
		ide_channel_io_wait(channel);
		int ret = ide_channel_poll(channel, ATA_SR_DRQ, ATA_SR_DRQ);
		if (ret < 0) return ret;

		ret = ide_channel_transfer(channel, command);
		if (ret < 0) return ret;
	}

	// the irq handler will take care of the rest
	return 0;
}

static int ide_channel_poll_mode(ide_channel_t *channel, ata_command_t *command) {
	// TODO : DMA support
	while (channel->bytes_transferred < command->buf_size) {
		ide_channel_io_wait(channel);
		int ret = ide_channel_poll(channel, ATA_SR_BSY | ATA_SR_DRQ, ATA_SR_DRQ);
		if (ret < 0) return ret;
	
		ret = ide_channel_transfer(channel, command);
		if (ret < 0) return ret;
	}
	
	ide_channel_io_wait(channel);
	int ret = ide_channel_poll(channel, ATA_SR_BSY, 0);
	if (ret < 0) return ret;

	atomic_store(&channel->current_command, NULL);
	ioreq_finish(&command->ioreq, 0);
	return 0;
}

static int ide_channel_raw_send_ata_command(ide_channel_t *channel, ata_device_t *device, ata_command_t *command) {
	int ret = ide_channel_poll(channel, ATA_SR_BSY, 0);
	if (ret < 0) return ret;
	
	// reset channel state tracking
	channel->bytes_transferred = 0;

	// select the drive
	// TODO : don't reselect if is was already selected
	uint8_t drv_select = ATA_DRV_SELECT_LEGACY;
	if (device == channel->slave) {
		drv_select |= ATA_DRV_SELECT_SLAVE;
	}
	drv_select |= command->regs.device;
	ide_channel_write(channel, IDE_REG_DRV_SELECT, drv_select);

	ide_channel_io_wait(channel);
	uint8_t status = ide_channel_read(channel, IDE_REG_STATUS);
	if (status == 0 || status == 0xff) {
		// no drive
		return -ENODEV;
	}
	ret = ide_channel_poll(channel, ATA_SR_BSY, 0);
	if (ret < 0) return ret;

	if (command->flags & ATA_CMD_SEND_LBA48) {
		ide_channel_write(channel, IDE_REG_SECCOUNT0, command->regs.sectors_count1);
		ide_channel_write(channel, IDE_REG_LBA0, command->regs.lba3);
		ide_channel_write(channel, IDE_REG_LBA1, command->regs.lba4);
		ide_channel_write(channel, IDE_REG_LBA2, command->regs.lba5);
	}

	if (command->flags & (ATA_CMD_SEND_LBA28 | ATA_CMD_SEND_LBA48)) {
		ide_channel_write(channel, IDE_REG_SECCOUNT0, command->regs.sectors_count0);
		ide_channel_write(channel, IDE_REG_LBA0, command->regs.lba0);
		ide_channel_write(channel, IDE_REG_LBA1, command->regs.lba1);
		ide_channel_write(channel, IDE_REG_LBA2, command->regs.lba2);
	}

	ide_channel_write(channel, IDE_REG_COMMAND, command->regs.command);

	if (command->flags & ATA_CMD_PACKET_PROTOCOL) {
		ide_channel_io_wait(channel);
		ret = ide_channel_poll(channel, ATA_SR_BSY | ATA_SR_DRQ, ATA_SR_DRQ);
		if (ret < 0) return ret;
		ide_channel_send_packet(channel, command);
	}

	if (channel->irq_handler) {
		return ide_channel_irq_mode(channel, command);
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
