#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/bus.h>
#include <kernel/irq.h>
#include <module/pci.h>
#include <module/pnpdevice.h>

typedef struct isa_ioport {
	size_t start;
	size_t size;
	int valid;
} isa_ioport_t;

#define ISA_IOPORT(start, size) {start, size, 1}

typedef hwirq_t isa_irq_t;
#define ISA_IRQ(x) x
#define ISA_MAX_IOPORT 10
#define ISA_MAX_IRQ    10

typedef struct isa_probe {
	const char *pnp_id;
	isa_ioport_t ioports[ISA_MAX_IOPORT];
	isa_irq_t    irqs[ISA_MAX_IRQ];
} isa_probe_t;

static isa_probe_t isa_probes[] = {
	// IDE controller
	{"PNP0600", {
			ISA_IOPORT(0x1f0, 8),
			ISA_IOPORT(0x3f4, 4),
			ISA_IOPORT(0x170, 8),
			ISA_IOPORT(0x374, 4),
		}, {
			ISA_IRQ(14),
			ISA_IRQ(15),
		}
	},

	// COM1
	{"PNP0501",  {
			ISA_IOPORT(0x3f8, 8),
		}, {
			ISA_IRQ(4),
		}
	},

	// COM2
	{"PNP0501",  {
			ISA_IOPORT(0x2f8, 8),
		}, {
			ISA_IRQ(3),
		}
	},

};

static int isa_check(devnode_t *devnode) {
	// there are two cases
	// we can be hardcoded in root
	// or found through a PCI to ISA bridge
	if (devnode->type != BUS_PCI) {
		// child of root (hardcoded)
		return 1;
	}
	pci_dev_t *pci_dev = container_of(devnode, pci_dev_t, devnode);
	if (pci_dev->class == 0x6 && pci_dev->subclass == 0x1) {
		// PCI to ISA bridge
		return 1;
	}
	return 0;
}

static int isa_probe(devnode_t *isa_bus) {
	// attach a bunch of hardcoded children
	for (size_t i=0; i<arraylen(isa_probes); i++) {
		isa_probe_t *probe = &isa_probes[i];

		pnp_device_t *device = kmalloc(sizeof(pnp_device_t));
		if (!device) return -ENOMEM;
		memset(device, 0, sizeof(pnp_device_t));
		device->pnp_id       = probe->pnp_id;
		device->devnode.type = BUS_ISA;
		
		// add ioports
		for (size_t j=0; j<ISA_MAX_IOPORT; j++) {
			if (!probe->ioports[j].valid) continue;
			bus_add_fixed_resource_desc(&device->devnode, probe->ioports[j].start, probe->ioports[j].size, RESOURCE_IOPORT, PNP_RID_IOPORT(j));
		}

		// add irqs
		for (size_t j=0; j<ISA_MAX_IRQ; j++) {
			if (!probe->irqs[j]) continue;
			irq_t *irq = irq_get_from_hwirq(main_irq_chip, probe->irqs[j]);
			if (!irq) continue;
			bus_add_fixed_resource_desc(&device->devnode, irq->hwirq, 1, RESOURCE_IRQ, PNP_RID_IRQ(j));
		}
		bus_attach_child(isa_bus, &device->devnode, NULL, UNIT_NOUNIT);
	}
	return 0;
}

static driver_t isa_driver = {
	.name = "ISA bus driver",
	.device_name = "isa",
	.buses = BUSES("root", "pci"),
	.check = isa_check,
	.probe = isa_probe,
};

int isa_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	return driver_register(&isa_driver);
}

int isa_fini(void) {
	return driver_unregister(&isa_driver);
}

kmodule_t module_meta = {
	.magic       = MODULE_MAGIC,
	.init        = isa_init,
	.fini        = isa_fini,
	.author      = "tayoky",
	.name        = "isa",
	.description = "legacy ISA driver",
	.license     = "GPL 3",
};
