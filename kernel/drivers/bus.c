#include <kernel/bus.h>
#include <kernel/devclass.h>
#include <kernel/slab.h>
#include <errno.h>

static slab_cache_t resources_slab;
static slab_cache_t devnodes_slab;
static devnode_t *root_bus = NULL;
static list_t drivers;
list_t devnodes;

void bus_set_root(devnode_t *bus) {
	root_bus = bus;
}

devnode_t *bus_get_root(void) {
	return root_bus;
}

void init_bus(void) {
	slab_init(&resources_slab, sizeof(resource_t), "resources");
	slab_init(&devnodes_slab, sizeof(devnode_t), "devnodes");
	init_devclass();

	// create root dev
	root_bus = slab_alloc(&devnodes_slab);
	memset(root_bus, 0, sizeof(devnode_t));
	device_set_name(root_bus, "root", UNIT_NOUNIT);
	list_append(&devnodes, &root_bus->list_node);
}

static void device_attempt_attatch_with(devnode_t *device, driver_t *driver) {
	// recurse
	foreach (node, &device->children) {
		devnode_t *child = container_of(node, devnode_t, node);
		device_attempt_attach_with(child, driver);
	}

	// only try if unattached
	if (!device_has_driver_attached(device)) {
		device_attach_driver(device, driver);
	}
}

int driver_register(driver_t *driver) {
	driver->devclass = devclass_get_or_create(driver->device_name);
	if (!driver->devclass) {
		return -ENOMEM;
	}
	list_append(&drivers, &driver->node);
	// try this driver on already existing devnodes
	device_attempt_attach_with(root_bus, driver); 
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
	list_append(&devnodes, &child->list_node);
	child->parent = bus;
	if (name) {
		child->devclass = devclass_create_or_get(name);
		child->unit = unit;
		devclass_allocate_unit(child->devclass, child);
		child->flags |= DEVNODE_FIXEDNAME;
	}

	// can we find a driver for this device ?
	device_attach_driver_auto(child);
	return child;
}

void bus_delete_child(devnode_t *bus, devnode_t *child) {
	device_detach_driver(child);
	devclass_free_unit(child->devclass, child);
	list_remove(&devnodes, &child->list_node);
	list_remove(&bus->children, &child->node);
	slab_free(child);
}

int device_check_driver(devnode_t *device, driver_t *driver) {
	if (device->flags & DEVNODE_FIXEDNAME) {
		// the driver must have a matching devclass
		if (device->devclass != driver->devclass) {
			return -ENOTSUP;
		}
	}
	if (!driver->check || !driver->probe) return -ENOTSUP;
	if (!driver->check(device)) return -ENOTSUP;
	return 0;
}

static void device_generate_cached_name(devnode_t *devnode, device) {
	snprintf(devnode->cached_name, sizeof(devnode->cached_name), devnode->devclass->name, devnode->unit);
}

int device_attach_driver(devnode_t *device, driver_t *driver) {
	if (!driver) return -EINVAL;
	// does the driver support the device ?
	if (!device_check_driver(device, driver)) return -ENOTSUP;
	
	if (device_has_attached_driver(device)) {
		// a driver already control this device
		if (device->driver->priority >= driver->priority) {
			// the old driver is already better
			return -EBUSY;
		} else {
			// replace the old driver
			device_detach_driver(device);
		}
	}

	// setup a default name acording to the name specified on the driver
	if (device->devclass != driver->devclass) {
		// remove old devclass
		devclass_free_unit(device->devclass, device);
		// set devclass and allocate unit
		device->devclass = devclass;
		device->unit = UNIT_ALLOCATE;
		devclass_alloc_unit(devclass, device);
		device_generate_cached_name(device);
	}

	// the driver is compatible with the device
	int ret = driver->probe(device);
	if (ret >= 0) {
		device->driver = driver;
	}
	return ret;
}

int device_attach_driver_auto(devnode_t *device) {
	driver_t *best = NULL;
	int best_priority = 0;
	foreach (node, &drivers) {
		driver_t *driver = container_of(node, driver_t, node);
		if (driver->priority >= best_priority && device_check_driver(device, driver) >= 0) {
			best = driver;
			best_priority = driver->priority;
		}
	}
	device_attach_driver(device, best);
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
	if (device->devclass == devclass && device->unit == unit) {
		return;
	}
	// remove old devclass
	devclass_free_unit(device->devclass, device);
	device->devclass = devclass;
	device->unit = unit;
	devclass_alloc_unit(devclass, device);
	device_generate_cached_name(device);
}

char *device_get_dup_name(device_t *device) {
	char *name = kmalloc(strlen(device->name) + 5);
	sprintf(name, device->devclass->name, device->unit);
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
