#ifndef KERNEL_BUS_H
#define KERNEL_BUS_H

#include <kernel/device.h>
#include <kernel/devclass.h>
#include <kernel/interrupt.h>
#include <kernel/resource.h>
#include <kernel/list.h>
#include <sys/types.h>
#include <errno.h>

struct driver;
struct device;

typedef struct devnode {
	list_node_t node;
	list_node_t list_node;
	list_t children;
	struct devnode *parent;
	list_t resources;
	struct device *device;
	struct driver *driver;
	devclass_t *devclass;
	char *name; // address name
	char cached_name[32];
	int unit;
	int flags;
	int type;
} devnode_t;

#define DEVNODE_FIXEDNAME 0x01 // name gaved by bus

typedef struct driver {
	list_node_t node;
	list_node_t list_node;
	const char *name;
	const char *device_name;
	devclass_t *devclass;
	int priority;
	const char **buses; // suported buses
	int (*check)(devnode_t *devnode);
	int (*probe)(devnode_t *devnode);
	void (*detach)(devnode_t *devnode);
	resource_t *(*allocate_resource)(devnode_t *bus, devnode_t *devnode, size_t start, size_t count, int flags, int rid);
	int (*release_resource)(devnode_t *bus, devnode_t *devnode, resource_t *resource);
	int (*activate_resource)(devnode_t *bus, devnode_t *devnode, resource_t *resource);
	int (*deactivate_resource)(devnode_t *bus, devnode_t *devnode, resource_t *resource);
} driver_t;

#define BUSES(...) (const char *[]){__VA_ARGS__, NULL}

#define BUS_PCI 1
#define BUS_PS2 2
#define BUS_USB 3

int driver_register(driver_t *driver);
int driver_unregister(driver_t *driver);

devnode_t *bus_attach_child(devnode_t *bus, devnode_t *child, const char *name, int unit);
void bus_delete_child(devnode_t *bus, devnode_t *child);

devnode_t *device_allocate(void);
int device_check_driver(devnode_t *device, driver_t *driver);
int device_attach_driver(devnode_t *device, driver_t *driver);
int device_attach_driver_auto(devnode_t *device);
void device_detach_driver(devnode_t *device);
static inline int device_has_attached_driver(devnode_t *device) {
	return device->driver != NULL;
}
int device_set_name(devnode_t *device, const char *name, int unit);
char *device_get_dup_name(devnode_t *device);
static inline const char *device_get_name(devnode_t *device) {
	return device->cached_name;
}

static inline void bus_attach_resource(devnode_t *devnode, resource_t *resource) {
	list_append(&devnode->resources, &resource->node);
}

static inline void bus_detach_resource(devnode_t *devnode, resource_t *resource) {
	list_remove(&devnode->resources, &resource->node);
}

static inline void bus_attach_bound_resource(devnode_t *devnode, resource_t *resource) {
	resource->flags |= RESOURCE_BOUND;
	list_append(&devnode->resources, &resource->node);
}

static inline void bus_detach_bound_resource(devnode_t *devnode, resource_t *resource) {
	list_remove(&devnode->resources, &resource->node);
}

static inline resource_t *device_get_resource(devnode_t *devnode, int flags, int rid) {
	foreach (node, &devnode->resources) {
		resource_t *resource = container_of(node, resource_t, node);
		if ((resource->flags & RESOURCE_TYPE) == (flags & RESOURCE_TYPE) && ((rid == RID_ANY) || (resource->rid == rid))) {
			return resource;
		}
	}
	return NULL;
}

#define BUS_UPWARD_OP(bus, op, ...) \
	devnode_t *current = bus; \
	while (current) {\
		if (current->driver && current->driver->op) {\
			return current->driver->op(current,  __VA_ARGS__); \
		} \
		current = current->parent; \
	}

static inline int __helper_bus_activate_resource(devnode_t *bus, devnode_t *devnode, resource_t *resource) {
	BUS_UPWARD_OP(bus, activate_resource, devnode, resource);
	return -EINVAL;
}

static inline int bus_activate_resource(devnode_t *bus, devnode_t *devnode, resource_t *resource) {
	if (!resource) {
		return -EINVAL;
	} else if (resource->flags & RESOURCE_ACTIVE) {
		return 0;
	}
	int ret = __helper_bus_activate_resource(bus, devnode, resource);
	if (ret < 0) return ret;
	resource->flags |= RESOURCE_ACTIVE;
	return 0;
}

static inline void bus_deactivate_resource(devnode_t *bus, devnode_t *devnode, resource_t *resource) {
	if (!resource) return;
	if (!(resource->flags & RESOURCE_ACTIVE)) {
		return;
	}
	resource->flags &= ~RESOURCE_ACTIVE;
	BUS_UPWARD_OP(bus, deactivate_resource, devnode, resource);
}

static inline void bus_release_resource(devnode_t *bus, devnode_t *devnode, resource_t *resource) {
	if (!resource || IS_ERR(resource)) return;
	if (resource->flags & RESOURCE_ACTIVE) {
		bus_deactivate_resource(bus, devnode, resource);
	}
	if (resource->flags & RESOURCE_BOUND) {
		return;
	}
	bus_detach_resource(devnode, resource);
	BUS_UPWARD_OP(bus, release_resource, devnode, resource);
}

static inline resource_t *__helper_bus_allocate_resource(devnode_t *bus, devnode_t *devnode, size_t start, size_t count, int flags, int rid) {
	BUS_UPWARD_OP(bus, allocate_resource, devnode, start, count, flags, rid);
	return ERR2PTR(-EINVAL);
}

static inline resource_t *bus_allocate_resource(devnode_t *bus, devnode_t *devnode, size_t start, size_t count, int flags, int rid) {
	// maybee we already have a resource for this rid
	resource_t *resource = device_get_resource(devnode, flags, rid);
	if (resource) return resource;
	resource = __helper_bus_allocate_resource(bus, devnode, start, count, flags & ~RESOURCE_ACTIVE, rid);
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

static inline resource_t *bus_allocate_count_resource(devnode_t *bus, devnode_t *devnode, size_t count, int flags, int rid) {
	return bus_allocate_resource(bus, devnode, RESOURCE_ANY_START, count, flags, rid);
}

static inline resource_t *bus_allocate_simple_resource(devnode_t *bus, devnode_t *devnode, int flags, int rid) {
	return bus_allocate_resource(bus, devnode, RESOURCE_ANY_DTART, RESOURCE_ANY_SIZE, flags, rid);
}

static inline resource_t *device_allocate_resource(devnode_t *devnode, size_t start, size_t count, int flags, int rid) {
	return bus_allocate_resource(devnode->parent, devnode, start, count, flags, rid);
}

static inline resource_t *device_allocate_count_resource(devnode_t *devnode, size_t count, int flags, int rid) {
	return device_allocate_resource(devnode, RESOURCE_ANY_START, count, flags, rid);
}

static inline resource_t *device_allocate_simple_resource(devnode_t *devnode, int flags, int rid) {
	return bus_allocate_resource(devnode, RESOURCE_ANY_DTART, RESOURCE_ANY_SIZE, flags, rid);
}

static inline void device_release_resource(devnode_t *devnode, resource_t *resource) {
	return bus_release_resource(devnode->parent, devnode, resource);
}

static inline void device_activate_resource(devnode_t *devnode, resource_t *resource) {
	return bus_activate_resource(devnode->parent, devnode, resource);
}

static inline void device_deactivate_resource(devnode_t *devnode, resource_t *resource) {
	return bus_deactivate_resource(devnode->parent, devnode, resource);
}

void bus_set_root(devnode_t *bus);
devnode_t *bus_get_root(void);
void init_bus(void);
extern list_t devnodes;

#endif
