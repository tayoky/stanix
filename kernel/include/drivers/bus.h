#ifndef KERNEL_BUS_H
#define KERNEL_BUS_H

#include <kernel/device.h>
#include <kernel/interrupt.h>
#include <kernel/list.h>
#include <sys/types.h>
#include <errno.h>

struct bus_ops;

typedef struct bus {
	device_t device;
	list_t addresses;
	struct bus_ops *ops;
} bus_t;

typedef struct devnode {
	list_node_t node;
	list_t resources;
	device_t *device;
	char *name;
	bus_t *bus;
	int type;
} devnode_t;

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
	void *(*register_handler)(bus_t *bus, devnode_t *devnode, resource_t *resource, interrupt_handler_t handler, void *data);
	void (*unregister_handler)(bus_t *bus, devnode_t *devnode, resource_t *resource, void *handle);
} bus_ops_t;

static inline void bus_attach_resource(devnode_t *devnode, resource_t *resource) {
	list_append(&addr->resources, &resource->node);
}

static inline void bus_attach_bound_resource(devnode_t *devnode, resource_t *resource) {
	resource->flags |= RESOURCE_BOUND;
	list_append(&addr->resources, &resource->node);
}

static inline void bus_detach_resource(devnode_t *devnode, resource_t *resource) {
	list_remove(&addr->resources, &resource->node);
}

static inline resource_t *bus_get_resource(devnode_t *devnode, int flags, int rid) {
	foreach (node, &addr->resources) {
		resource_t *resource = container_of(node, resource_t, node);
		if ((resource->flags & RESOURCE_TYPE) == (flags & RESOURCE_TYPE) && (rid == RID_ANY) || (resource->rid == rid)) {
			return resource;
		}
	}
	return NULL;
}

#define BUS_UPWARD_OP(addr, op, ...) \
	devnode_t *current = addr; \
	while (current) {\
		bus_t *bus = current->bus;\
		kassert(bus);\
		if (bus->ops->op) {\
			return bus->ops->op(bus, addr, __VA_ARGS__); \
		} \
		current = bus->device.addr; \
	}

static inline int __helper_bus_activate_resource(devnode_t *devnode, resource_t *resource) {
	BUS_UPWARD_OP(addr, activate_resource, resource);
	return -EINVAL;
}

static inline int bus_activate_resource(devnode_t *devnode, resource_t *resource) {
	if (!resource) {
		return -EINVAL;
	} else if (resource->flags & RESOURCE_ACTIVE) {
		return 0;
	}
	int ret = __helper_bus_activate_resource(addr, resource);
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
	BUS_UPWARD_OP(addr, deactivate_resource, resource);
}

static inline void bus_release_resource(devnode_t *devnode, resource_t *resource) {
	if (!resource) return;
	if (resource->flags & RESOURCE_ACTIVE) {
		bus_deactivate_resource(addr, resource);
	}
	BUS_UPWARD_OP(addr, deactivate_resource, resource);
}

static inline resource_t *__helper_bus_allocate_resource(devnode_t *devnode, size_t start, size_t count, int flags, int rid) {
	BUS_UPWARD_OP(addr, allocate_resource, addr, start, count, flags, rid);
	return ERR2PTR(-EINVAL);
}

static inline resource_t *bus_allocate_resource(devnode_t *devnode, size_t start, size_t count, int flags, int rid) {
	// maybee we already have a resource for this rid
	resource_t *resource = bus_get_resource(addr, flags, rid);
	if (resource) return resource;
	resource = __helper_bus_allocate_resource(addr, start, count, flags & ~RESOURCE_ACTIVE, rid);
	if (IS_ERR(resource)) {
		return resource;
	}
	if (flags & RESOURCE_ACTIVE) {
		int ret = bus_activate_resource(addr, resource);
		if (ret < 0) {
			bus_release_resource(addr, resource);
			return ERR2PTR(ret);
		}
	}
	return resource;
}

static inline resource_t *bus_allocate_count_resource(devnode_t *devnode, size_t count, int flags, rid) {
	return bus_allocate_resource(addr, 0, count, flags, rid);
}

static inline resource_t *bus_allocate_simple_resource(devnode_t *devnode, int flags, int rid) {
	return bus_allocate_resource(addr, 0, 0, flags, rid);
}

/**
 * @brief register an handler for an IRQ resource.
 * @param addr TODO
 * @param resource the resource to register the handler for
 * @param handler the handle to call on interrupt
 * @param data data pointer to pass to the handler
 * @return an handle to pass to \ref resource_unregister_handler on success or NULL on failure
 * @note all handlers are unregistered automaticaly on \ref bus_release_resource
 */
static inline void *bus_register_handler(devnode_t *devnode, resource_t *resource, interrupt_handler_t handler, void *data) {
	if (!handler) return NULL;
	BUS_UPWARD_OP(addr, register_handler, resource, handler, data);
	return NULL;
}

/**
 * @brief unregitser an handler previously registered wuth \ref resource_register_handler
 * @param addr TODO
 * @param resource the resource to unregister a handler for
 * @param handle the handle of the handler to unregister
 */
static inline void bus_unregister_handler(devnode_t *devnode, resource_t *resource, void *handle) {
	if (!handle) return;
	BUS_UPWARD_OP(addr, unregister_handler, resource, handle);
}

// TODO : remove this shit
static inline int bus_old_register_handler(devnode_t *devnode, interrupt_handler_t handler, void *data) {
	if (!addr->bus->ops || !addr->bus->ops->old_register_handler) return -ENOTSUP;
	return addr->bus->ops->old_register_handler(addr, handler, data);
}

static inline ssize_t bus_old_read(devnode_t *devnode, void *buf, off_t offset, size_t size) {
	if (!addr->bus->ops || !addr->bus->ops->read) return -ENOTSUP;
	return addr->bus->ops->read(addr, buf, offset, size);
}

static inline ssize_t bus_old_write(devnode_t *devnode, const void *buf, off_t offset, size_t size) {
	if (!addr->bus->ops || !addr->bus->ops->write) return -ENOTSUP;
	return addr->bus->ops->write(addr, buf, offset, size);
}

void bus_set_root(bus_t *bus);
bus_t *bus_get_root(void);
void init_bus(void);

#endif
