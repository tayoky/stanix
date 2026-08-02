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
	case RESOURCE_IRQ:
		kassert(count == 1);
		return resource_allocate(flags, rid, start, count);
	case RESOURCE_IOPORT:
		return rman_allocate(&io_rman, devnode, start, count, flags);
	case RESOURCE_MEMORY:
		return resource_allocate(flags, rid, start, count);
	default:
		return ERR2PTR(-ENOTSUP);
	}
}

static void root_release_resource(devnode_t *bus, devnode_t *devnode, resource_t *resource) {
	(void)bus;
	(void)devnode;
	switch (flags & RESOURCE_TYPE) {
	case RESOURCE_IRQ:
		slab_free(resource);
		return;
	case RESOURCE_IOPORT:
		rman_free(&io_rman, resource);
		return;
	case RESOURCE_MEMORY:
		slab_free(resource);
		return;
	default:
		return;
	}
}

static int root_activate_resource(devnode_t *bus, devnode_t *devnode, resource_t *resource) {
	(void)bus;
	(void)devnode;
	switch (flags & RESOURCE_TYPE) {
	case RESOURCE_MEMORY:
		resource->data = mmio_map(resource->start, resource->size);
		if (!resource->data) {
			return -EIO;
		}
		return 0;
	default:
		return 0;
	}
}

static void root_deactivate_resource(devnode_t *bus, devnode_t *devnode, resource_t *resource) {
	(void)bus;
	(void)devnode;
	switch (flags & RESOURCE_TYPE) {
	case RESOURCE_MEMORY:
		mmio_unmap(resource->data, resource->size);
		resource->data = NULL;
		return;
	default:
		return;
	}
}

static driver_t root_driver = {
	.name = "root bus",
	.device_name = "root",
	.check = root_check,
	.probe = root_probe,
	.allocate_resource   = root_allocate_resource,
	.release_resource    = root_release_resource,
	.activate_resource   = root_activate_resource,
	.deactivate_resource = root_deactivate_resource,
};

void init_root_bus(void) {
	kstatusf("init root bus...");
	driver_register(&root_driver);
	kok();
}
