#include <kernel/arch.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/irq.h>
#include <kernel/bus.h>

static int root_check(devnode_t *devnode) {
	// we can only drive root
	if (devnode != bus_get_root()) return -ENOTSUP;
	return 0;
}

static int root_probe(devnode_t *devnode) {
	// TODO : add resources, pci bus, isa/acpi, ...
	return 0;
}

static driver_t root_driver = {
	.name = "root bus",
	.device_name = "root",
	.check = root_check,
	.probe = root_probe,
};

void init_root_bus(void) {
	kstatusf("init root bus...");
	driver_register(&root_driver);
	kok();
}
