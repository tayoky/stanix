#include <kernel/arch.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/irq.h>
#include <kernel/bus.h>

static rman_t io_rman;

static int root_check(devnode_t *devnode) {
	// we can only drive root
	if (devnode != bus_get_root()) return 0;
	return 1;
}

static int root_probe(devnode_t *devnode) {
	// TODO : add  pci bus, isa/acpi, ...
	rman_init(&io_rman, RESOURCE_IOPORT, "IO ports");
	rman_add_region(&io_rman, 0, 0xffff);
	rman_set_dynamic_start(&io_rman, 0x1000);
	return 0;
}

static resource_t *root_allocate_resource(devnode_t *bus, devnode_t *devnode, size_t start, size_t count, int flags, int rid) {
	(void)bus;
	(void)rid;
	switch (flags & RESOURCE_TYPE) {
	case RESOURCE_IOPORT:
		return rman_allocate(&io_rman, devnode, start, count, flags);
	default:
		return ERR2PTR(-ENOTSUP);
	}
}

static driver_t root_driver = {
	.name = "root bus",
	.device_name = "root",
	.check = root_check,
	.probe = root_probe,
	.allocate_resource = root_allocate_resource,
};

void init_root_bus(void) {
	kstatusf("init root bus...");
	driver_register(&root_driver);
	kok();
}
