#include <kernel/bus.h>
#include <kernel/devclass.h>
#include <kernel/kheap.h>
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
	root_bus = device_allocate();
	memset(root_bus, 0, sizeof(devnode_t));
	device_set_name(root_bus, "root", UNIT_NOUNIT);
	list_append(&devnodes, &root_bus->list_node);
}

devnode_t *device_allocate(void) {
	devnode_t *device = slab_alloc(&devnodes_slab);
	if (!device) return NULL;
	memset(device, 0, sizeof(devnode_t));
	return device;
}

static void device_attempt_attach_with(devnode_t *device, driver_t *driver) {
	// recurse
	foreach (node, &device->children) {
		devnode_t *child = container_of(node, devnode_t, node);
		device_attempt_attach_with(child, driver);
	}

	// only try if unattached
	if (!device_has_attached_driver(device)) {
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

static void device_print_name(devnode_t *devnode, char *buf, size_t size) {
	const char *fmt = devnode->devclass->name;
	char *ptr = buf;
	while (*fmt && size > 1) {
		if (*fmt == '%') {
			fmt++;
			switch (*fmt) {
			case 'd':
				snprintf(ptr, size, "%d", devnode->unit);
				size_t written = strlen(ptr);
				ptr += written;
				size -= written;
				break;
			case 'c':
				*(ptr++) = 'a' + devnode->unit;
				size--;
				break;
			case 'C':
				*(ptr++) = 'A' + devnode->unit;
				size--;
				break;
			case '%':
				*(ptr++) = '%';
				size--;
				break;
			}
		} else {
			*(ptr++) = *fmt;
			size--;
		}
		fmt++;
	}
	if (size > 0) {
		*ptr = '\0';
	}
}

static void device_generate_cached_name(devnode_t *devnode) {
	device_print_name(devnode, devnode->cached_name, sizeof(devnode->cached_name));
}

devnode_t *bus_attach_child(devnode_t *bus, devnode_t *child, const char *name, int unit) {
	if (!child) {
		// allocate one ourself
		child = device_allocate();
		if (!child) return NULL;
	}

	list_append(&bus->children, &child->node);
	list_append(&devnodes, &child->list_node);
	child->parent = bus;
	if (name) {
		child->devclass = devclass_get_or_create(name);
		child->unit = unit;
		devclass_alloc_unit(child->devclass, child);
		child->flags |= DEVNODE_FIXEDNAME;
		device_generate_cached_name(child);
	}

	kinfof("attached device %p(%s) child of %p(%s)\n", child, device_get_name(child), bus, device_get_name(bus));

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

static resource_t *__helper_bus_allocate_resource(devnode_t *bus, devnode_t *devnode, resource_request_t *request, int rid) {
	BUS_UPWARD_OP(bus, allocate_resource, devnode, request, rid);
	return ERR2PTR(-EINVAL);
}

resource_t *bus_allocate_resource(devnode_t *bus, devnode_t *devnode, resource_request_t *request, int rid) {
	// maybee we already have a resource for this rid
	resource_t *resource = device_get_resource(devnode, request->flags, rid);
	if (resource) return resource;

	// maybee we have a desc for this rid
	resource_desc_t *desc = device_get_resource_desc(devnode, request->flags, rid);
	int flags = request->flags;
	if (desc) {
		kdebugf("got resource desc start=%zx size=%zu\n", desc->request.start, desc->request.size);
		if (request->start == RESOURCE_ANY_START) {
			request->start = desc->request.start;
		}
		if (request->size == RESOURCE_ANY_SIZE) {
			request->size = desc->request.size;
		}
		if (request->align == RESOURCE_ANY_ALIGN) {
			request->align = desc->request.align;
		}
		if (request->bound == RESOURCE_ANY_BOUND) {
			request->bound = desc->request.bound;
		}
		request->flags |= desc->request.flags & ~RESOURCE_TYPE;
	}
	request->flags &= ~RESOURCE_ACTIVE;
	resource = __helper_bus_allocate_resource(bus, devnode, request, rid);
	if (IS_ERR(resource)) {
		return resource;
	}
	resource->rid = rid;
	bus_attach_resource(devnode, resource);
	if (flags & RESOURCE_ACTIVE) {
		int ret = bus_activate_resource(bus, devnode, resource);
		if (ret < 0) {
			bus_release_resource(bus, devnode, resource);
			return ERR2PTR(ret);
		}
	}
	return resource;
}

static int driver_support_bus(driver_t *driver, devclass_t *bus_type) {
	for (const char **current = driver->buses; current && *current; current++) {
		if (!strcmp(*current, bus_type->name)) {
			return 1;
		}
	}
	return 0;
}

int device_check_driver(devnode_t *device, driver_t *driver) {
	if (device->flags & DEVNODE_FIXEDNAME) {
		// the driver must have a matching devclass
		if (device->devclass != driver->devclass) {
			return -ENOTSUP;
		}
	}
	if (device->parent && !driver_support_bus(driver, device->parent->devclass)) {
		return -ENOTSUP;
	}
	if (!driver->probe) {
		return -ENOTSUP;
	}

	if (driver->check) {
		if (!driver->check(device)) return -ENOTSUP;
	} else {
		// the driver uses fixed name
		if (!(device->flags & DEVNODE_FIXEDNAME)) return -ENOTSUP;
	}
	return 0;
}

int device_attach_driver(devnode_t *device, driver_t *driver) {
	if (!driver) return -EINVAL;
	// does the driver support the device ?
	if (device_check_driver(device, driver) < 0) return -ENOTSUP;
	
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
		device->devclass = driver->devclass;
		device->unit = UNIT_ALLOCATE;
		devclass_alloc_unit(driver->devclass, device);
		device_generate_cached_name(device);
	}

	// the driver is compatible with the device
	device->driver = driver;
	int ret = driver->probe(device);
	if (ret < 0) {
		device->driver = NULL;
	} else {
		kinfof("attached driver %s to device %p(%s)\n", driver->name, device, device_get_name(device));
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
	return 0;
}

int device_detach_driver(devnode_t *device) {
	if (!device_has_attached_driver(device)) {
		return 0;
	}
	if (!device->driver->detach) {
		kwarningf("driver %s cannot be detached\n", device->driver->name);
		return -ENOTSUP;
	}
	device->driver->detach(device);
	// detach device
	if (device->device) {
		device->device->devnode = NULL;
		device->device = NULL;
	}
	device->driver = NULL;
	return 0;
}

int device_set_name(devnode_t *device, const char *name, int unit) {
	devclass_t *devclass = devclass_get_or_create(name);
	if (!devclass) return -ENOMEM;
	if (device->devclass == devclass && device->unit == unit) {
		return 0;
	}
	// remove old devclass
	devclass_free_unit(device->devclass, device);
	device->devclass = devclass;
	device->unit = unit;
	devclass_alloc_unit(devclass, device);
	kdebugf("devclass=%p device=%p\n", devclass, device);
	device_generate_cached_name(device);
	return 0;
}

char *device_get_dup_name(devnode_t *device) {
	size_t len = strlen(device->devclass->name) + 16;
	char *name = kmalloc(len);
	if (!name) return NULL;
	device_print_name(device, name, len);
	return name;
}
