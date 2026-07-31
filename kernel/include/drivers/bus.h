#ifndef KERNEL_BUS_H
#define KERNEL_BUS_H

#include <kernel/device.h>
#include <kernel/interrupt.h>
#include <kernel/resource.h>
#include <kernel/list.h>
#include <sys/types.h>
#include <errno.h>

struct bus_ops;
struct driver;
struct device;
struct devclass;

// OLD
typedef struct bus {
	device_t device;
	list_t addresses;
	struct bus_ops *ops;
} bus_t;

typedef struct devnode {
	list_node_t node;
	list_t children;
	struct devnode *parent;
	list_t resources;
	struct device *device;
	struct driver *driver;
	bus_t *bus; // old
	struct devclass *devclass;
	int unit;
	int type;
} devnode_t;

typedef struct driver {
	list_node_t node;
	const char *name;
	const char *device_name;
	struct devclass *devclass;
	int priority;
	const char **buses; // suported buses
	int (*check)(devnode_t *devnode);
	int (*probe)(devnode_t *devnode);
	void (*detach)(devnode_t *devnode);
	ssize_t (*old_read)(devnode_t *devnode, void *buf, off_t offset, size_t size);
	ssize_t (*old_write)(devnode_t *devnode, const void *buf, off_t offset, size_t size);
	int (*old_register_handler)(devnode_t *devnode, interrupt_handler_t handler, void *data);
	resource_t *(*allocate_resource)(bus_t *bus, devnode_t *devnode, size_t start, size_t count, int flags, int rid);
	void (*release_resource)(bus_t *bus, devnode_t *devnode, resource_t *resource);
	int (*activate_resource)(bus_t *bus, devnode_t *devnode, resource_t *resource);
	void (*deactivate_resource)(bus_t *bus, devnode_t *devnode, resource_t *resource);
} bus_ops_t;
} driver_t;

#define BUSES(...) (const char *[]){__VA_ARGS__, NULL}

#define BUS_PCI 1
#define BUS_PS2 2
#define BUS_USB 3

typedef struct bus_ops {
	ssize_t (*read)(devnode_t *devnode, void *buf, off_t offset, size_t size);
	ssize_t (*write)(devnode_t *devnode, const void *buf, off_t offset, size_t size);
	int (*old_register_handler)(devnode_t *devnode, interrupt_handler_t handler, void *data);
	resource_t *(*allocate_resource)(bus_t *bus, devnode_t *devnode, size_t start, size_t count, int flags, int rid);
	void (*release_resource)(bus_t *bus, devnode_t *devnode, resource_t *resource);
	int (*activate_resource)(bus_t *bus, devnode_t *devnode, resource_t *resource);
	void (*deactivate_resource)(bus_t *bus, devnode_t *devnode, resource_t *resource);
} bus_ops_t;

int driver_register(driver_t *driver);
int driver_unregister(driver_t *driver);

devnode_t *bus_attach_child(devnode_t *bus, devnode_t *child, const char *name, int unit);
void bus_delete_child(devnode_t *bus, devnode_t *child);

int device_check_driver(devnode_t *device, driver_t *driver);
int device_attach_driver(devnode_t *device, driver_t *driver);
void device_detach_driver(devnode_t *device);
static inline int device_has_driver_attached(devnode_t *device) {
	return device->driver != NULL;
}
int device_set_name(devnode_t *device, const char *name, int unit);
char *device_get_dup_name(device_t *device);

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

static inline resource_t *bus_get_resource(devnode_t *devnode, int flags, int rid) {
	foreach (node, &devnode->resources) {
		resource_t *resource = container_of(node, resource_t, node);
		if ((resource->flags & RESOURCE_TYPE) == (flags & RESOURCE_TYPE) && (rid == RID_ANY) || (resource->rid == rid)) {
			return resource;
		}
	}
	return NULL;
}

#define BUS_UPWARD_OP(devnode, op, ...) \
	devnode_t *current = devnode; \
	while (current) {\
		bus_t *bus = current->bus;\
		kassert(bus);\
		if (bus->ops->op) {\
			return bus->ops->op(bus, devnode, __VA_ARGS__); \
		} \
		current = bus->device.devnode; \
	}

static inline int __helper_bus_activate_resource(devnode_t *devnode, resource_t *resource) {
	BUS_UPWARD_OP(devnode, activate_resource, resource);
	return -EINVAL;
}

static inline int bus_activate_resource(devnode_t *devnode, resource_t *resource) {
	if (!resource) {
		return -EINVAL;
	} else if (resource->flags & RESOURCE_ACTIVE) {
		return 0;
	}
	int ret = __helper_bus_activate_resource(devnode, resource);
	if (ret < 0) return ret;
	resource->flags |= RESOURCE_ACTIVE;
	return 0;
}

static inline void bus_deactivate_resource(devnode_t *devnode, resource_t *resource) {
	if (!resource) return;
	if (!(resource->flags & RESOURCE_ACTIVE)) {
		return;
	}
	resource->flags &= ~RESOURCE_ACTIVE;
	BUS_UPWARD_OP(devnode, deactivate_resource, resource);
}

static inline void bus_release_resource(devnode_t *devnode, resource_t *resource) {
	if (!resource) return;
	if (resource->flags & RESOURCE_ACTIVE) {
		bus_deactivate_resource(devnode, resource);
	}
	BUS_UPWARD_OP(devnode, deactivate_resource, resource);
}

static inline resource_t *__helper_bus_allocate_resource(devnode_t *devnode, size_t start, size_t count, int flags, int rid) {
	BUS_UPWARD_OP(devnode, allocate_resource, start, count, flags, rid);
	return ERR2PTR(-EINVAL);
}

static inline resource_t *bus_allocate_resource(devnode_t *devnode, size_t start, size_t count, int flags, int rid) {
	// maybee we already have a resource for this rid
	resource_t *resource = bus_get_resource(devnode, flags, rid);
	if (resource) return resource;
	resource = __helper_bus_allocate_resource(devnode, start, count, flags & ~RESOURCE_ACTIVE, rid);
	if (IS_ERR(resource)) {
		return resource;
	}
	if (flags & RESOURCE_ACTIVE) {
		int ret = bus_activate_resource(devnode, resource);
		if (ret < 0) {
			bus_release_resource(devnode, resource);
			return ERR2PTR(ret);
		}
	}
	return resource;
}

static inline resource_t *bus_allocate_count_resource(devnode_t *devnode, size_t count, int flags, int rid) {
	return bus_allocate_resource(devnode, 0, count, flags, rid);
}

static inline resource_t *bus_allocate_simple_resource(devnode_t *devnode, int flags, int rid) {
	return bus_allocate_resource(devnode, 0, 0, flags, rid);
}

// TODO : remove this shit
static inline int bus_old_register_handler(devnode_t *devnode, interrupt_handler_t handler, void *data) {
	if (!devnode->bus->ops || !devnode->bus->ops->old_register_handler) return -ENOTSUP;
	return devnode->bus->ops->old_register_handler(devnode, handler, data);
}

static inline ssize_t bus_old_read(devnode_t *devnode, void *buf, off_t offset, size_t size) {
	if (!devnode->bus->ops || !devnode->bus->ops->read) return -ENOTSUP;
	return devnode->bus->ops->read(devnode, buf, offset, size);
}

static inline ssize_t bus_old_write(devnode_t *devnode, const void *buf, off_t offset, size_t size) {
	if (!devnode->bus->ops || !devnode->bus->ops->write) return -ENOTSUP;
	return devnode->bus->ops->write(devnode, buf, offset, size);
}

void bus_set_root(devnode_t *bus);
devnode_t *bus_get_root(void);
void init_bus(void);

#endif
