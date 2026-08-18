#include <kernel/bus.h>
#include <kernel/irq.h>
#include <kernel/devclass.h>
#include <kernel/kheap.h>
#include <kernel/module.h>
#include <kernel/port.h>
#include <kernel/print.h>
#include <kernel/string.h>
#include <module/pci.h>
#include <errno.h>
#include <stdint.h>

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA    0xCFC

/**
 * @brief take an device bus function and offset and turn it into an conf addr
 * @param bus
 * @param device
 * @param function
 * @param offset
 * @return the configuration address
 */
static inline uint32_t addr2conf_addr(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
	return (((uint32_t)bus) << 16) | (((uint32_t)device) << 11) | (((uint32_t)function) << 8) | offset | ((uint32_t)0x80000000);
}

uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
	// check alignement
	kassert(offset % 4 == 0);

	uint32_t addr = addr2conf_addr(bus, device, function, offset);
	out_long(CONFIG_ADDRESS, addr);
	return in_long(CONFIG_DATA);
}

void pci_config_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t data) {
	// check alignement
	kassert(offset % 4 == 0);

	uint32_t addr = addr2conf_addr(bus, device, function, offset);
	out_long(CONFIG_ADDRESS, addr);
	out_long(CONFIG_DATA, data);
}

uint16_t pci_config_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
	// check alignement
	kassert(offset % 2 == 0);

	uint32_t data = pci_config_read32(bus, device, function, offset & ~3U);
	int shift     = (offset % 4) * 8;
	return (uint16_t)((data >> shift) & 0xffff);
}

void pci_config_write16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t data) {
	// check alignement
	kassert(offset % 2 == 0);

	uint32_t new_data = pci_config_read32(bus, device, function, offset & ~3U);
	int shift         = (offset % 4) * 8;
	new_data &= ~(0xffffU << shift);
	new_data |= data << shift;
	pci_config_write32(bus, device, function, offset & ~3U, new_data);
}

uint8_t pci_config_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
	uint32_t data = pci_config_read32(bus, device, function, offset & ~3U);
	int shift     = (offset % 4) * 8;
	return (uint8_t)((data >> shift) & 0xff);
}

void pci_config_write8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t data) {
	uint32_t new_data = pci_config_read32(bus, device, function, offset & ~3U);
	int shift         = (offset % 4) * 8;
	new_data &= ~(0xffU << shift);
	new_data |= data << shift;
	pci_config_write32(bus, device, function, offset & ~3U, new_data);
}

static void check_bus(uint8_t bus, void (*func)(uint8_t, uint8_t, uint8_t, void *), void *);

static void check_function(uint8_t bus, uint8_t device, uint8_t function, void (*func)(uint8_t, uint8_t, uint8_t, void *), void *arg) {
	if (func) {
		func(bus, device, function, arg);
	}

	uint8_t base_class = (pci_config_read16(bus, device, function, PCI_CONFIG_CLASS) >> 8) & 0xFF;
	uint8_t sub_class  = pci_config_read16(bus, device, function, PCI_CONFIG_CLASS) & 0xFF;
	uint8_t secondary_bus;

	// check for PCI to PCI bridge
	if ((base_class == 0x6) && (sub_class == 0x4)) {
		secondary_bus = (pci_config_read16(bus, device, function, PCI_CONFIG_BUS_NUMBER) >> 8) & 0xFF;
		check_bus(secondary_bus, func, arg);
	}
}

static void check_device(uint8_t bus, uint8_t device, void (*func)(uint8_t, uint8_t, uint8_t, void *), void *arg) {
	uint8_t function = 0;

	// read the vendor id of the device
	uint16_t vendorID = pci_config_read16(bus, device, function, PCI_CONFIG_VENDOR_ID);
	if (vendorID == 0xFFFF) {
		// device doesn't exist
		return;
	}

	// now we know the device exist check every single function of the device
	check_function(bus, device, function, func, arg);
	// read the header type
	uint32_t header_type = pci_config_read16(bus, device, function, PCI_CONFIG_HEADER_TYPE);
	if (header_type & 0x80) {
		// it's a multi-function device, so check remaining functions
		for (function = 1; function < 8; function++) {
			if (pci_config_read16(bus, device, function, PCI_CONFIG_VENDOR_ID) != 0xFFFF) {
				check_function(bus, device, function, func, arg);
			}
		}
	}
}

static void check_bus(uint8_t bus, void (*func)(uint8_t, uint8_t, uint8_t, void *), void *arg) {
	uint8_t device;

	for (device = 0; device < 32; device++) {
		check_device(bus, device, func, arg);
	}
}

void pci_foreach(void (*func)(uint8_t, uint8_t, uint8_t, void *), void *arg) {
	uint8_t function;

	uint32_t headerType = pci_config_read16(0, 0, 0, PCI_CONFIG_HEADER_TYPE);
	if ((headerType & 0x80) == 0) {
		// single PCI host controller
		check_bus(0, func, arg);
	} else {
		// multiple PCI host controllers
		// each function belong to a PCI host controller
		for (function = 0; function < 8; function++) {
			if (pci_config_read16(0, 0, function, PCI_CONFIG_VENDOR_ID) != 0xFFFF) {
				check_bus(function, func, arg);
			}
		}
	}
}

uintptr_t pci_get_bar(pci_dev_t *addr, int ioport, int BAR) {
	uintptr_t BAR_low  = pci_config_read32(addr->bus, addr->device, addr->function, PCI_CONFIG_BAR0 + BAR * 4);
	uintptr_t BAR_high = pci_config_read32(addr->bus, addr->device, addr->function, PCI_CONFIG_BAR0 + BAR * 4 + 4);
	if (BAR_low & 1) {
		// io port
		if (ioport) {
			return BAR_low & 0xfffffffc;
		} else {
			return PCI_INVALID_BAR;
		}
	}

	if (ioport) return PCI_INVALID_BAR;

	switch ((BAR_low & 0b110) >> 1) {
	case 0x02:
		// 64 bits BAR
		return (BAR_low & 0xfffffff0) | (BAR_high << 32);
	case 0x00:
		// 32 bits BAR
		return BAR_low & 0xfffffff0;
	case 0x01:
		// 16 bits BAR
		return BAR_low & 0xfff0;
	default:
		return PCI_INVALID_BAR;
	}
}

static size_t parse_bar(pci_dev_t *pci_dev, int bar) {
	uint64_t bar_value = pci_config_read32(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CONFIG_BAR0 + bar * 4);
	int is_ioport      = bar_value & 0x1;
	int is_64bits      = 0;
	if (!is_ioport && (bar_value & 0x6) == 0x4) {
		is_64bits = 1;

		if (bar == 5) {
			// bar 5 is last and cannot be 64 bits
			return 1;
		}

		// we need to read the higher part
		uint64_t bar_high = pci_config_read32(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CONFIG_BAR0 + (bar + 1) * 4);
		bar_value |= bar_high << 32;
	}

	uint64_t base;
	if (is_ioport) {
		base = bar_value & ~0x3ULL;
	} else {
		base = bar_value & ~0xfULL;
	}

	if (base == 0 || (is_64bits ? (bar_value == UINT64_MAX) : (bar_value == UINT32_MAX))) {
		base = RESOURCE_ANY_START;
	}

	pci_config_write32(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CONFIG_BAR0 + bar * 4, 0xffffffff);
	if (is_64bits) {
		pci_config_write32(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CONFIG_BAR0 + (bar + 1) * 4, 0xffffffff);
	}

	uint64_t readback;
	if (is_64bits) {
		uint32_t readback_low  = pci_config_read32(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CONFIG_BAR0 + bar * 4);
		uint32_t readback_high = pci_config_read32(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CONFIG_BAR0 + (bar + 1) * 4);
		readback               = ((uint64_t)readback_high << 32) | readback_low;
	} else {
		readback = pci_config_read32(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CONFIG_BAR0 + bar * 4);
	}

	// restore
	pci_config_write32(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CONFIG_BAR0 + bar * 4, bar_value);
	if (is_64bits) {
		pci_config_write32(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CONFIG_BAR0 + (bar + 1) * 4, bar_value >> 32);
	}

	// mask the control bits
	if (is_ioport) {
		readback &= ~0x3ULL;
	} else {
		readback &= ~0xfULL;
	}

	if (readback == 0) {
		// unimplemented BAR
		goto finish;
	}

	size_t bar_size = (~readback) + 1;

	size_t end;
	if (base == RESOURCE_ANY_START) {
		// we cannot allocate 32 bits bar after the 4GB limit
		end = is_64bits ? UINT64_MAX : UINT32_MAX;
	} else {
		end = base + bar_size;
	}

	resource_request_t request = {
		.start = base,
		.end   = end,
		.size  = bar_size,
		.align = bar_size,
	};

	if (is_ioport) {
		// io port
		request.flags = RESOURCE_IOPORT;
		if (request.align < 4) {
			// minimum align : 4
			request.align = 4;
		}
	} else {
		// memory
		request.flags = RESOURCE_MEMORY;
		if (request.align < 16) {
			// minimum align : 16
			request.align = 16;
		}
	}
	bus_add_resource_desc(&pci_dev->devnode, &request, PCI_RID_BAR(bar));
finish:
	return is_64bits ? 2 : 1;
}

static int write_bar(pci_dev_t *pci_dev, int bar, resource_t *resource) {
	uint32_t bar_value = pci_config_read32(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CONFIG_BAR0 + bar * 4);
	int is_64bits      = 0;
	int is_ioport      = bar_value & 0x1;
	uint32_t mask;
	if (is_ioport) {
		// ioport
		if ((resource->flags & RESOURCE_TYPE) != RESOURCE_IOPORT) {
			return -EINVAL;
		}
		mask = 0x3;
	} else {
		// memory
		if ((resource->flags & RESOURCE_TYPE) != RESOURCE_MEMORY) {
			return -EINVAL;
		}

		if ((bar_value & 0x6) == 0x4) {
			// 64 bits
			is_64bits = 1;
		}
		mask = 0xf;
	}

	// write the actual BAR
	pci_config_write32(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CONFIG_BAR0 + bar * 4, resource->start | (bar_value & mask));
	if (is_64bits) {
		pci_config_write32(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CONFIG_BAR0 + (bar + 1) * 4, (uint32_t)(resource->start >> 32));
	}
	return 0;
}

static void write_irq_line(pci_dev_t *pci_dev, resource_t *resource) {
	irq_t *irq = (irq_t *)resource->start;
	pci_config_write8(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CONFIG_INT_LINE, irq->irqnum);
}

static void parse_msi(pci_dev_t *pci_dev) {
	// on startup disable msi and mask everything
	uint8_t message_control = pci_config_read8(pci_dev->bus, pci_dev->device, pci_dev->function, pci_dev->msi_offset + PCI_CAP_MSI_MSG_CONTROL);
	message_control &= ~PCI_CAP_MSI_MSG_CONTROL_ENABLE;
	message_control |= PCI_CAP_MSI_MSG_CONTROL_64BIT;
	pci_config_write8(pci_dev->bus, pci_dev->device, pci_dev->function, pci_dev->msi_offset + PCI_CAP_MSI_MSG_CONTROL, message_control);

	pci_config_write32(pci_dev->bus, pci_dev->device, pci_dev->function, pci_dev->msi_offset + PCI_CAP_MSI_MASK, 0xffffffff);

	uint8_t interrupts_count = 1U << ((message_control >> 1) & 0x7U);
	kinfof("found msi with %hhu interrupts\n", interrupts_count);
	resource_request_t request = {
		.start = IRQ_MSI_START,
		.end   = IRQ_MSI_END,
		.size  = interrupts_count,
		.align = interrupts_count,
		.flags = RESOURCE_IOPORT,
	};
	bus_add_resource_desc(&pci_dev->devnode, &request, PCI_RID_MSI);
}

static void write_msi(pci_dev_t *pci_dev, resource_t *resource) {
	irq_t *irq = resource_get_irq(resource, 0);
	uintptr_t addr = irq_msi_get_address(irq);
	uint32_t data  = irq_msi_get_data(irq);
	pci_config_write32(pci_dev->bus, pci_dev->device, pci_dev->function, pci_dev->msi_offset + PCI_CAP_MSI_MSG_ADDR_LOW,  addr & 0xffffffff);
	pci_config_write32(pci_dev->bus, pci_dev->device, pci_dev->function, pci_dev->msi_offset + PCI_CAP_MSI_MSG_ADDR_HIGH, (addr >> 32) & 0xffffffff);
	pci_config_write32(pci_dev->bus, pci_dev->device, pci_dev->function, pci_dev->msi_offset + PCI_CAP_MSI_MSG_DATA, data);
}

static void parse_capabilities(pci_dev_t *pci_dev) {
	// we have a capability list
	uint8_t current = pci_config_read8(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CONFIG_CAPABILITIES) & ~3U;
	while (current) {
		uint8_t id = pci_config_read8(pci_dev->bus, pci_dev->device, pci_dev->function, current);
		switch (id) {
		case PCI_CAP_MSI:
			if (pci_dev->msi_offset) {
				// multiple msi capabilities ???
				// invalid
				kwarningf("multiple msi capabilities");
				return;
			}
			pci_dev->msi_offset = current;
			break;
		}
		current = pci_config_read8(pci_dev->bus, pci_dev->device, pci_dev->function, current + 1) & ~3U;
	}
	if (pci_dev->msi_offset) {
		parse_msi(pci_dev);
	}
}

static void create_pci_dev(uint8_t bus, uint8_t device, uint8_t function, void *arg) {
	devnode_t *pci_bus = arg;
	uint16_t vendorID  = pci_config_read16(bus, device, function, PCI_CONFIG_VENDOR_ID);
	uint16_t deviceID  = pci_config_read16(bus, device, function, PCI_CONFIG_DEVICE_ID);
	kdebugf("pci : find bus %d device %d function %d vendorID : %lx deviceID : %lx\n", bus, device, function, vendorID, deviceID);

	char name[64];
	sprintf(name, "%d:%d:%d", bus, device, function);

	// setup the pci_dev
	pci_dev_t *pci_dev = kmalloc(sizeof(pci_dev_t));
	memset(pci_dev, 0, sizeof(pci_dev_t));
	pci_dev->devnode.type = BUS_PCI;
	pci_dev->devnode.name = strdup(name);
	pci_dev->device_id    = deviceID;
	pci_dev->vendor_id    = vendorID;
	pci_dev->class        = pci_config_read8(bus, device, function, PCI_CONFIG_BASE_CLASS);
	pci_dev->subclass     = pci_config_read8(bus, device, function, PCI_CONFIG_SUB_CLASS);
	pci_dev->prog_if      = pci_config_read8(bus, device, function, PCI_CONFIG_PROG_IF);
	pci_dev->bus          = bus;
	pci_dev->device       = device;
	pci_dev->function     = function;

	// resource discovery time
	for (int i = 0; i < 6;) {
		i += parse_bar(pci_dev, i);
	}
	uint8_t status = pci_config_read8(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CONFIG_STATUS);
	if (status & PCI_STATUS_CAPABILITY_LIST) {
		parse_capabilities(pci_dev);
	}

	// discover irq line
	bus_add_size_resource_desc(&pci_dev->devnode, 1, RESOURCE_IRQ, PCI_RID_IRQ_LINE);

	bus_attach_child(pci_bus, &pci_dev->devnode, NULL, UNIT_NOUNIT);
}

static resource_t *pci_allocate_resource(devnode_t *pci_bus, devnode_t *devnode, resource_request_t *request, int rid) {
	// ask our parent for the resource
	if (devnode->parent != pci_bus) {
		// not a child of us just passthough
		return bus_allocate_resource(pci_bus->parent, devnode, request, RID_NONE);
	}
	resource_t *resource = bus_allocate_resource(pci_bus->parent, devnode, request, RID_NONE);
	if (IS_ERR(resource)) {
		return resource;
	}
	pci_dev_t *pci_dev = container_of(devnode, pci_dev_t, devnode);
	switch (resource->flags & RESOURCE_TYPE) {
	case RESOURCE_IRQ:
		if (rid == PCI_RID_IRQ_LINE) {
			write_irq_line(pci_dev, resource);
		} else if (rid == PCI_RID_MSI) {
			write_msi(pci_dev, resource);
		}
		break;
	case RESOURCE_IOPORT:
	case RESOURCE_MEMORY:
		if (rid >= PCI_RID_BAR0 && rid <= PCI_RID_BAR5) {
			write_bar(pci_dev, rid - PCI_RID_BAR0, resource);
		}
		break;
	}
	return resource;
}

// TODO : export this
static ssize_t pci_read(devnode_t *devnode, void *buf, off_t offset, size_t size) {
	pci_dev_t *pci_dev = container_of(devnode, pci_dev_t, devnode);

	// only allow word aligned read
	if (offset % 2 || size % 2) return -EINVAL;

	if (size + offset > 256) size = 256 - offset;
	if (offset >= 256) return 0;

	uint16_t *buffer = buf;
	for (size_t i = 0; i < (size / 2); i++) {
		*(buffer++) = pci_config_read16(pci_dev->bus, pci_dev->device, pci_dev->function, offset);
		offset += 2;
	}

	return size;
}

static int pci_probe(devnode_t *devnode) {
	pci_foreach(create_pci_dev, devnode);
	return 0;
}


static driver_t pci_driver = {
	.name              = "pci",
	.device_name       = "pci",
	.buses             = BUSES("root"),
	.probe             = pci_probe,
	.allocate_resource = pci_allocate_resource,
};

int init_pci(int argc, char **argv) {
	(void)argc;
	(void)argv;

	driver_register(&pci_driver);

	// hardly add pci as child to root
	bus_attach_child(bus_get_root(), NULL, "pci", UNIT_NOUNIT);

	EXPORT(pci_foreach);
	EXPORT(pci_config_read32)
	EXPORT(pci_config_read16)
	EXPORT(pci_config_read8)
	EXPORT(pci_config_write32)
	EXPORT(pci_config_write16)
	EXPORT(pci_config_write8)
	EXPORT(pci_get_bar)
	return 0;
}

int fini_pci() {
	driver_unregister(&pci_driver);
	UNEXPORT(pci_foreach);
	UNEXPORT(pci_config_read32)
	UNEXPORT(pci_config_read16)
	UNEXPORT(pci_config_read8)
	UNEXPORT(pci_config_write32)
	UNEXPORT(pci_config_write16)
	UNEXPORT(pci_config_write8)
	UNEXPORT(pci_get_bar)
	return 0;
}

kmodule_t module_meta = {
	.magic       = MODULE_MAGIC,
	.init        = init_pci,
	.fini        = fini_pci,
	.name        = "pci",
	.author      = "tayoky",
	.description = "pci driver",
	.license     = "GPL 3",
};
