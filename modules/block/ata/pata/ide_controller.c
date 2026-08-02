#include <kernel/bus.h>
#include <kernel/kheap.h>
#include <module/pci.h>
#include <module/isa.h>
#include <ide.h>

#define PROG_IF_CHANNEL1_NATIVE         (1U << 0)
#define PROG_IF_CHANNEL1_SWITCH_CAPABLE (1U << 1)
#define PROG_IF_CHANNEL2_NATIVE         (1U << 2)
#define PROG_IF_CHANNEL1_SWITCH_CAPABLE (1U << 3)
#define PROG_IF_BUS_MASTERING           (1U << 7)

static int ide_controller_pci_check(devnode_t *devnode) {
	pci_dev_t *pci_dev = container_of(devnode, pci_dev_t, devnode);
	if (devnode->type != BUS_PCI) return 0;
	if ((pci_dev->class == 1) && ((pci_dev->subclass == 5) || (pci_dev->subclass == 1))) {
		kdebugf("found IDE controller on %d:%d:%d\n", pci_dev->bus, pci_dev->device, pci_dev->function);
		return 1;
	}
	return 0;
}

static int ide_controller_pci_probe(devnode_t *devnode) {
	pci_dev_t *pci_dev = container_of(devnode, pci_dev_t, devnode);
	uint8_t prog_if    = pci_dev->prog_if;
	uint8_t bus        = pci_dev->bus;
	uint8_t device     = pci_dev->device;
	uint8_t function   = pci_dev->function;

	// we prefer PCI native mode
	if (!(prog_if & PROG_IF_CHANNEL1_NATIVE)) {
		// primary channel pci is in compatibility
		// can we switch ?
		if (prog_if & PROG_IF_CHANNEL1_SWITCH_CAPABLE) {
			prog_if |= PROG_IF_CHANNEL1_NATIVE;
		}
	}
	if (!(prog_if & PROG_IF_CHANNEL2_NATIVE)) {
		// secondary channel is in compatibility
		// can we switch ?
		if (prog_if & PROG_IF_CHANNEL2_SWITCH_CAPABLE) {
			prog_if |= PROG_IF_CHANNEL2_NATIVE;
		}
	}

	// write new prog if
	pci_write_config_byte(bus, device, function, PCI_CONFIG_PROG_IF, prog_if);
	prog_if = pci_dev->prog_if = pci_read_config_byte(bus, device, function, PCI_CONFIG_PROG_IF);

	// check resources
	if (prog_if & PROG_IF_CHANNEL1_NATIVE) {
		controller->base1 = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, PCI_RID_BAR0);
		controller->ctrl1 = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, PCI_RID_BAR1);
	} else {
		controller->base1 = device_allocate_start_resource(devnode, 0x1f0, 8, RESOURCE_IOPORT, RID_NONE);
		controller->ctrl1 = device_allocate_start_resource(devnode, 0x3f4, 4, RESOURCE_IOPORT, RID_NONE);
	}

	if (prog_if & PROG_IF_CHANNEL2_NATIVE) {
		controller->base2 = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, PCI_RID_BAR2);
		controller->ctrl2 = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, PCI_RID_BAR3);
	} else {
		controller->base2 = device_allocate_start_resource(devnode, 0x170, 8, RESOURCE_IOPORT, RID_NONE);
		controller->ctrl2 = device_allocate_start_resource(devnode, 0x374, 4, RESOURCE_IOPORT, RID_NONE);
	}

	if (prog_if & PROG_IF_BUS_MASTERING) {
		controller->bmide = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, PCI_RID_BAR4);
	}

	return 0;
}

static int ide_controller_isa_check(devnode_t *devnode) {
	(void)devnode;
	// the ISA bus is hardcoded
	// which mean we only get called on the ISA IDE device
	return 1;
}

static int ide_controller_isa_probe(devnode_t *devnode) {
	// since the ISA bus is hardcoded, resources are always here
	// no need to check anything
	controller->base1 = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, ISA_RID_IOPORT0);
	controller->ctrl1 = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, ISA_RID_IOPORT1);
	controller->base2 = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, ISA_RID_IOPORT2);
	controller->ctrl2 = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, ISA_RID_IOPORT3);
	controller->bmide = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, ISA_RID_IOPORT4);
	return 0;
}

static int ide_controller_check(devnode_t *devnode) {
	switch (devnode->type) {
	case BUS_PCI:
		return ide_controller_pci_check(devnode);
	case BUS_ISA:
		return ide_controller_isa_check(devnode);
	default:
		return 0;
	}
}

static int ide_controller_probe(devnode_t *devnode) {
	ide_controller_t *controller = kmalloc(sizeof(ide_controller_t));
	if (!controller) return -ENOMEM;
	memset(controller, 0, sizeof(ide_controller_t));
	devnode->private = controller;
	int ret;
	switch (devnode->type) {
	case BUS_PCI:
		ret = ide_controller_pci_probe(devnode);
		break;
	case BUS_ISA:
		ret = ide_controller_isa_probe(devnode);
		break;
	default:
		ret = -ENOTSUP;
		break;
	}
	if (ret < 0) {
		kfree(controller);
		return ret;
	}

	// create children
	if (!IS_ERR(controller->base1) && !IS_ERR(controller->ctrl1)) {
		devnode_t *channel1 = devnode_allocate();
		bus_add_resource_spec(channel1, controller->base1->start, controller->base1->size, RESOURCE_IOPORT, IDE_RID_BASE);
		bus_add_resource_spec(channel1, controller->ctrl1->start, controller->ctrl1->size, RESOURCE_IOPORT, IDE_RID_CTRL);
		if (controller->bmide && !IS_ERR(controller->bmide)) {
			bus_add_resource_spec(channel1, controller->bmide->start, 8, RESOURCE_IOPORT, IDE_RID_BMIDE);
		}
		bus_attach_children(devnose, channel1, "channel%d", 1);
	}
	if (!IS_ERR(controller->base2) && !IS_ERR(controller->ctrl2)) {
		devnode_t *channel2 = devnode_allocate();
		bus_add_resource_spec(channel2, controller->base2->start, controller->base2->size, RESOURCE_IOPORT, IDE_RID_BASE);
		bus_add_resource_spec(channel2, controller->ctrl2->start, controller->ctrl2->size, RESOURCE_IOPORT, IDE_RID_CTRL);
		if (controller->bmide && !IS_ERR(controller->bmide)) {
			bus_add_resource_spec(channel2, controller->bmide->start + 8, 8, RESOURCE_IOPORT, IDE_RID_BMIDE);
		}
		bus_attach_children(devnose, channel2, "channel%d", 2);
	}
	return 0;
}

static resource_t *ide_controller_allocate_resource(devnode_t *bus, devnode_t *devnode, resource_request_t *request, int rid) {
	if ((request->flags & RESOURCE_TYPE) == RESOURCE_IOPORT && rid >= IDE_RID_BASE && rid <= IDE_RID_BMIDE) {
		// the controller init already verified and allocated the resources
		// no need to redo ir
		return resource_allocate_request(request, rid);
	}
	// passthrough
	return bus_allocate_resource(bus->parent, devnode, request, rid);
}

static void ide_controller_release_resource(devnode_t *bus, devnode_t *devnode, resource_t *resource) {
	if ((resource->flags & RESOURCE_TYPE) == RESOURCE_IOPORT && rid >= IDE_RID_BASE && rid <= IDE_RID_BMIDE) {
		// we bypassed parent allocation for this
		// we need to bypass parent release too
		return slab_free(resource);
	}

	// passthrough
	return bus_release_resource(bus->parent, devnode, resource);
}

static driver_t ide_driver = {
	.name        = "IDE",
	.device_name = "ide%d",
	.buses       = BUSES("isa", "pci"),
	.check       = ide_controller_check,
	.probe       = ide_controller_probe,
	.allocate_resource = ide_controller_allocate_resource,
	.release_resource  = ide_controller_release_resource,
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
