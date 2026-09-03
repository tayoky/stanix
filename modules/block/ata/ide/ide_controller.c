#include <kernel/bus.h>
#include <kernel/kheap.h>
#include <module/pci.h>
#include <module/isa.h>
#include <ide.h>

#define PROG_IF_CHANNEL1_NATIVE         (1U << 0)
#define PROG_IF_CHANNEL1_SWITCH_CAPABLE (1U << 1)
#define PROG_IF_CHANNEL2_NATIVE         (1U << 2)
#define PROG_IF_CHANNEL2_SWITCH_CAPABLE (1U << 3)
#define PROG_IF_BUS_MASTERING           (1U << 7)

int disable_irq = 0;

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
	ide_controller_t *controller = devnode->private;
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
	pci_config_write8(bus, device, function, PCI_CONFIG_PROG_IF, prog_if);
	prog_if = pci_dev->prog_if = pci_config_read8(bus, device, function, PCI_CONFIG_PROG_IF);

	// check resources
	if (prog_if & PROG_IF_CHANNEL1_NATIVE) {
		controller->channel_res[0].base = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, PCI_RID_BAR0);
		controller->channel_res[0].ctrl = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, PCI_RID_BAR1);
	} else {
		controller->channel_res[0].base = device_allocate_fixed_resource(devnode, 0x1f0, 8, RESOURCE_IOPORT, RID_NONE);
		controller->channel_res[0].ctrl = device_allocate_fixed_resource(devnode, 0x3f4, 4, RESOURCE_IOPORT, RID_NONE);
		controller->channel_res[0].irq  = device_allocate_fixed_resource(devnode, 14, 1, RESOURCE_IRQ, RID_NONE);
	}

	if (prog_if & PROG_IF_CHANNEL2_NATIVE) {
		controller->channel_res[1].base = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, PCI_RID_BAR2);
		controller->channel_res[1].ctrl = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, PCI_RID_BAR3);
	} else {
		controller->channel_res[1].base = device_allocate_fixed_resource(devnode, 0x170, 8, RESOURCE_IOPORT, RID_NONE);
		controller->channel_res[1].ctrl = device_allocate_fixed_resource(devnode, 0x374, 4, RESOURCE_IOPORT, RID_NONE);
		controller->channel_res[1].irq  = device_allocate_fixed_resource(devnode, 15, 1, RESOURCE_IRQ, RID_NONE);
	}

	if (prog_if & (PROG_IF_CHANNEL1_NATIVE | PROG_IF_CHANNEL2_NATIVE)) {
		controller->shared_irq = device_allocate_simple_resource(devnode, RESOURCE_IRQ | RESOURCE_ACTIVE, PCI_RID_IRQ_LINE);
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
	ide_controller_t *controller = devnode->private;
	controller->channel_res[0].base = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, ISA_RID_IOPORT0);
	controller->channel_res[0].ctrl = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, ISA_RID_IOPORT1);
	controller->channel_res[1].base = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, ISA_RID_IOPORT2);
	controller->channel_res[1].ctrl = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, ISA_RID_IOPORT3);
	controller->channel_res[0].irq  = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, ISA_RID_IRQ(0));
	controller->channel_res[1].irq  = device_allocate_simple_resource(devnode, RESOURCE_IOPORT, ISA_RID_IRQ(1));
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

static void ide_controller_create_child(devnode_t *devnode, ide_controller_t *controller, int i) {
	if (IS_ERR(controller->channel_res[i].base) || IS_ERR(controller->channel_res[i].ctrl)) {
		return;
	}

	devnode_t *channel = device_allocate();
	bus_add_fixed_resource_desc(channel, controller->channel_res[i].base->start, controller->channel_res[i].base->size, RESOURCE_IOPORT, IDE_RID_BASE);
	bus_add_fixed_resource_desc(channel, controller->channel_res[i].ctrl->start, controller->channel_res[i].ctrl->size, RESOURCE_IOPORT, IDE_RID_CTRL);
	if (controller->bmide && !IS_ERR(controller->bmide)) {
		bus_add_fixed_resource_desc(channel, controller->bmide->start + i * 8, 8, RESOURCE_IOPORT, IDE_RID_BMIDE);
	}
	if (!disable_irq) {
		resource_t *irq = controller->channel_res[i].irq;
		if (!irq) irq = controller->shared_irq;
		if (irq && !IS_ERR(irq)) {
			bus_add_fixed_resource_desc(channel, irq->start, irq->size, RESOURCE_IRQ, IDE_RID_IRQ);
		}
	}

	bus_attach_child(devnode, channel, "ide_channel", devnode->unit * 2 + i);
}

static int ide_controller_probe(devnode_t *devnode) {
	ide_controller_t *controller = devnode->private;
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
	if (ret < 0) return ret;

	ide_controller_create_child(devnode, controller, 0);
	ide_controller_create_child(devnode, controller, 1);
	return 0;
}

static void ide_controller_detach(devnode_t *devnode) {
	ide_controller_t *controller = devnode->private;
	device_release_resource(devnode, controller->channel_res[0].base);
	device_release_resource(devnode, controller->channel_res[0].ctrl);
	device_release_resource(devnode, controller->channel_res[0].irq);
	device_release_resource(devnode, controller->channel_res[1].base);
	device_release_resource(devnode, controller->channel_res[1].ctrl);
	device_release_resource(devnode, controller->channel_res[1].irq);
	device_release_resource(devnode, controller->bmide);
	device_release_resource(devnode, controller->shared_irq);
}

static resource_t *ide_controller_allocate_resource(devnode_t *bus, devnode_t *devnode, resource_request_t *request, int rid) {
	if (((request->flags & RESOURCE_TYPE) == RESOURCE_IOPORT && rid >= IDE_RID_BASE && rid <= IDE_RID_BMIDE)
			|| ((request->flags & RESOURCE_TYPE) == RESOURCE_IRQ && rid == IDE_RID_IRQ)) {
		// the controller init already verified and allocated the resources
		// no need to redo it
		return resource_allocate_request(devnode, request, rid);
	}
	// passthrough
	return bus_allocate_resource(bus->parent, devnode, request, rid);
}

static int ide_controller_release_resource(devnode_t *bus, devnode_t *devnode, resource_t *resource) {
	if (((resource->flags & RESOURCE_TYPE) == RESOURCE_IOPORT && resource->rid >= IDE_RID_BASE && resource->rid <= IDE_RID_BMIDE)
			|| ((resource->flags & RESOURCE_TYPE) == RESOURCE_IRQ && resource->rid == IDE_RID_IRQ)) {
		// we bypassed parent allocation for this
		// we need to bypass parent release too
		resource_free(devnode, resource);
		return 0;
	}

	// passthrough
	return bus_release_resource(bus->parent, devnode, resource);
}

driver_t ide_controller_driver = {
	.name         = "IDE controller",
	.device_name  = "ide",
	.buses        = BUSES("isa", "pci"),
	.private_size = sizeof(ide_controller_t),
	.check        = ide_controller_check,
	.probe        = ide_controller_probe,
	.detach       = ide_controller_detach,
	.allocate_resource = ide_controller_allocate_resource,
	.release_resource  = ide_controller_release_resource,
};
