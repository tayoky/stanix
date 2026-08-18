#include <kernel/arch.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/slab.h>
#include <kernel/irq.h>
#include <kernel/bus.h>

static rman_t io_rman;

static int root_check(devnode_t *devnode) {
	// we can only drive root
	if (devnode != bus_get_root()) return 0;
	return 1;
}

static int root_probe(devnode_t *devnode) {
	(void)devnode;
	// TODO : add  pci bus, isa/acpi, ...
	rman_init(&io_rman, RESOURCE_IOPORT, "io-ports");
	rman_add_region(&io_rman, 0, 0xffff);
	return 0;
}

static resource_t *root_allocate_resource(devnode_t *bus, devnode_t *devnode, resource_request_t *request, int rid) {
	(void)bus;
	(void)rid;
	switch (request->flags & RESOURCE_TYPE) {
	case RESOURCE_IRQ:
		// by default, avoid allocating msi
		if (request->end == RESOURCE_ANY_END) {
			request->end = IRQ_MSI_START;
		}
		return rman_allocate(&main_irq_chip->rman, devnode, request);
	case RESOURCE_IOPORT:
		// by default start to dynamicly allocate from 0x1000
		if (request->start == RESOURCE_ANY_START) {
			request->start = 0x1000;
		}
		return rman_allocate(&io_rman, devnode, request);
	case RESOURCE_MEMORY:
		return resource_allocate_request(devnode, request, rid);
	default:
		return ERR2PTR(-ENOTSUP);
	}
}

static int root_release_resource(devnode_t *bus, devnode_t *devnode, resource_t *resource) {
	(void)bus;
	(void)devnode;
	switch (resource->flags & RESOURCE_TYPE) {
	case RESOURCE_IRQ:
		rman_free(&main_irq_chip->rman, devnode, resource);
		return 0;
	case RESOURCE_IOPORT:
		rman_free(&io_rman, devnode, resource);
		return 0;
	case RESOURCE_MEMORY:
		resource_free(devnode, resource);
		return 0;
	default:
		return 0;
	}
}

static int root_activate_resource(devnode_t *bus, devnode_t *devnode, resource_t *resource) {
	(void)bus;
	(void)devnode;
	switch (resource->flags & RESOURCE_TYPE) {
	case RESOURCE_MEMORY:
		resource->data = (void*)mmio_map(resource->start, resource->size);
		if (!resource->data) {
			return -EIO;
		}
		return 0;
	default:
		return 0;
	}
}

static int root_deactivate_resource(devnode_t *bus, devnode_t *devnode, resource_t *resource) {
	(void)bus;
	(void)devnode;
	switch (resource->flags & RESOURCE_TYPE) {
	case RESOURCE_MEMORY:
		mmio_unmap(resource->data, resource->size);
		resource->data = NULL;
		return 0;
	default:
		return 0;
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
