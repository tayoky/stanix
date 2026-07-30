#include <kernel/bus.h>
#include <kernel/devclass.h>
#include <kernel/slab.h>
#include <errno.h>

static slab_cache_t resources_slab;
static slab_cache_t devnodes_slab;
static bus_t *root_bus = NULL;
static list_t drivers;

void bus_set_root(bus_t *bus) {
	root_bus = bus;
}

bus_t *bus_get_root(void) {
	return root_bus;
}

void init_bus(void) {
	slab_init(&resources_slab, sizeof(resource_t), "resources");
	slab_init(&devnodes_slab, sizeof(devnode_t), "devnodes");
	init_devclass();
}

int driver_register(driver_t *driver) {
	driver->devclass = devclass_get_or_create(driver->device_name);
	if (!driver->devclass) {
		return -ENOMEM;
	}
	list_append(&drivers, &driver->node);
	// TODO : try this driver on already existing devnodes
	return 0;
}

int driver_unregister(driver_t *driver) {
	list_remove(&drivers, &driver->node);
	return 0;
}

devnode_t *bus_attach_child(devnode_t *bus, devnode_t *child, const char *name, int unit) {
	if (!child) {
		// allocate one ourself
		child = slab_alloc(&devnodes_slab);
		if (!child) return NULL;
		memset(child, 0, sizeof(devnode_t));
	}

	list_append(&bus->children, &child->node);
	child->parent = bus;
	if (name) {
		child->devclass = devclass_create_or_get(name);
		child->unit = unit;
		devclass_allocate_unit(child->devclass, child);
	}

	// can we find a driver for this device ?
	foreach (node, &drivers) {
		driver_t *driver = container_of(node, driver_t, node);
		device_attach_driver(child, driver);
	}
	return child;
}

void bus_delete_child(devnode_t *bus, devnode_t *child) {
	device_detach_driver(child);
	devclass_free_unit(child->devclass, child);
	list_remove(&bus->children, &child->node);
	slab_free(child);
}

int device_attach_driver(devnode_t *device, driver_t *driver) {
	// does the driver support the device ?
	if (!driver->check || !driver->probe) return -ENOTSUP;
	if (!driver->check(device)) return -ENOTSUP;
	
	if (device_has_attached_driver(device)) {
		// a driver already control this device
		if (device->driver->priority > device_driver->priority) {
			// the old driver is already better
			return -EBUSY;
		} else {
			// replace the old driver
			device_detach_driver(device);
		}
	}

	// the driver is compatible with the device
	int ret = driver->probe(device);
	if (ret >= 0) {
		device->driver = driver;

		// we might need to change unit/devclass
		if (device->devclass_free_unit(child->devclass, child);
	}
	return ret;
}

void device_detach_driver(devnode_t *device) {
	if (!device_has_driver_attached(device)) {
		return;
	}
	if (device->driver->detach) {
		device->driver->detach(device);
	}
	device->driver = NULL;
}

int device_set_name(devnode_t *device, const char *name, int unit) {
	devclass_t *devclass = devclass_get_or_create(name);
	if (!devclass) return -ENOMEM;
	if (device->devclass == devclass && 1) {
		return;
	}
	// TODO
}

resource_t *resource_allocate(int flags, int rid, size_t start, size_t size) {
	resource_t *resource = slab_alloc(&resources_slab);
	if (!resource) return NULL;
	memset(resource, 0, sizeof(resource_t));
	resource->flags = flags;
	resource->rid   = rid;
	resource->start = start;
	resource->size  = size;
	return resource;
}
