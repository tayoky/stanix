#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/bus.h>
#include <module/atah>
#include <error.h>
#include <ide.h>

static void ide_channel_write(ide_channel_t *channel, uint32_t reg, uint8_t data) {
	// set HOB
	if (reg > 0x07 && reg < 0x0C) {
		ide_channel_write(channel, ATA_REG_CONTROL, 0x80 | channel->nIEN);
	}
	if (reg <= ATA_REG_STATUS) {
		resource_write8(channel->base, reg, data);
	} else if (reg <= ATA_REG_LBA5) {
		resource_write8(channel->base, reg - 0x06, data);
	} else if (reg <= ATA_REG_DEVADDRESS) {
		resource_write8(channel->ctrl, reg - 0x0A, data);
	} else {
		resource_write8(channel->bmide, reg - 0xE, data);
	}

	if (reg > 0x07 && reg < 0x0C) {
		ide_channel_write(channel, ATA_REG_CONTROL, channel->nIEN);
	}
}

static uint8_t ide_channel_read(ide_channel_t *channel, uint32_t reg) {
	if (reg > 0x07 && reg < 0x0C) {
		ide_channel_write(channel, ATA_REG_CONTROL, 0x80 | channel->nIEN);
	}
	uint8_t data;
	if (reg <= ATA_REG_STATUS) {
		data = resource_read8(channel->base, reg);
	} else if (reg <= ATA_REG_LBA5) {
		data = resource_read8(channel->base, reg - 0x06);
	} else if (reg <= ATA_REG_DEVADDRESS) {
		data = resource_read8(channel->ctrl, reg - 0x0A);
	} else {
		data = resource_read8(channel->bmide, reg - 0xE);
	}
	if (reg > 0x07 && reg < 0x0C) {
		ide_channel_write(channel, ATA_REG_CONTROL, channel->nIEN);
	}
	return data;
}

static void ide_channel_io_wait(ide_channel_t *channel) {
	for (size_t i = 0; i < 4; i++) {
		ide_channel_read(channel, ATA_REG_ALTSTATUS);
	}
}

static int ide_channel_poll(ide_channel_t *channel, uint8_t mask, uint8_t value) {
	size_t timeout = 10000;
	while ((ide_channel_read(channel, ATA_REG_STATUS) & mask) != value) {
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
	ide_write(device, ATA_REG_CONTROL, 0x4 | device->channel->nIEN);
	ide_io_wait(device);
	ide_write(device, ATA_REG_CONTROL, device->channel->nIEN);
	mutex_release(&channel->mutex);
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
	
	ide_channel_reset(channel);
	return 0;
}

static int ide_channel_detach(devnode_t *devnode) {
	ide_channel_t *channel = devnode->private;
	device_release_resource(devnode, channel->base);
	device_release_resource(devnode, channel->ctrl);
	device_release_resource(devnode, channel->bmide);
	return 0;
}

driver_t *ide_channel_driver = {
	.name = "IDE channel",
	.device_name = "ide_channel%d",
	.probe = ide_channel_probe,
	.detach = ide_channel_detach,
};
