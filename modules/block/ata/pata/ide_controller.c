#include <kernel/ide.h>

// ide controller driver

typedef struct ide_controller {
} ide_controller_t;

static uint32_t reg2port(ide_channel_t *channel, uint32_t reg) {
	if (reg <= ATA_REG_STATUS) {
		return channel->base + reg;
	} else if (reg <= ATA_REG_LBA5) {
		return channel->base + reg - 0x06;
	} else if (reg <= ATA_REG_DEVADDRESS) {
		return channel->ctrl + reg - 0x0A;
	} else {
		return channel->bmide + reg - 0xE; // idk
	}
}

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

static void ide_io_wait(ide_channel_t *channel) {
	for (size_t i = 0; i < 4; i++) {
		ide_channel_read(channel, ATA_REG_ALTSTATUS);
	}
}

static int ide_poll(ide_channel_t *channel, uint8_t mask, uint8_t value) {
	size_t timeout = 10000;
	while ((ide_channel_read(channel, ATA_REG_STATUS) & mask) != value) {
		if (--timeout <= 0) {
			kwarningf("timeout expired\n");
			return -1;
		};
	}
	return 0;
}

static int ide_controller_check(devnode_t *devnode) {
	// TODO : ISA support
	pci_dev_t *pci_dev = container_of(devnode, pci_dev_t, devnode);
	if (devnode->type != BUS_PCI) return 0;
	if ((pci_dev->class == 1) && ((pci_dev->subclass == 5) || (pci_dev->subclass == 1))) {
		kdebugf("found IDE controller on %d:%d:%d\n", pci_dev->bus, pci_dev->device, pci_dev->function);
		return 1;
	}
	return 0;
}

static int ide_controller_probe(devnode_t *devnode) {
	// TODO : ISA support
	pci_dev_t *pci_dev = container_of(devnode, pci_dev_t, devnode);
	uint8_t prog_if    = pci_dev->prog_if;
	uint8_t bus        = pci_dev->bus;
	uint8_t device     = pci_dev->device;
	uint8_t function   = pci_dev->function;

	// TODO : support pci native mode
	// this would require the pci driver to implement dynamic bar allocation (at least for IO)
	// and register non initalized BAR (the bios do not initalize BAR0 BAR1 BAR2 and BAR3)
	if (prog_if & 0x1) {
		// primary channel pci native mode
		// can we switch ?
		if (!(prog_if & 0x02)) {
			kdebugf("ide controller don't support compatibility mode\n");
			return -ENOTSUP;
		}
		prog_if &= ~0x1;
	}
	if (prog_if & 0x4) {
		// secondary channel pci native mode
		// can we switch ?
		if (!(prog_if & 0x08)) {
			kdebugf("ide controller don't support compatibility mode\n");
			return -ENOTSUP;
		}
		prog_if &= ~0x4;
	}

	// write new prog if
	pci_dev->prog_if = prog_if;
	pci_write_config_byte(bus,device,function,PCI_CONFIG_PROG_IF,prog_if);

	// TODO : initalize controller, check and create child channels
	// TODO : we need the pci driver to tell us even non initalized BAR in order to discover channels
}

static driver_t ide_driver = {
	.name        = "IDE",
	.device_name = "ide%d",
	.buses       = BUSES("pci", "isa"),
	.check       = ide_controller_check,
	.probe       = ide_controller_probe,
};

int ide_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	driver_register(&ide_controller_driver);
	return 0;
}

int ide_fini() {
	driver_unregister(&ide_controller_driver);
	return 0;
}

kmodule_t module_meta = {
	.magic       = MODULE_MAGIC,
	.init        = ide_controller_init,
	.fini        = ide_controller_fini,
	.author      = "tayoky",
	.name        = "IDE controller",
	.description = "IDE controller driver",
	.license     = "GPL 3",
};
