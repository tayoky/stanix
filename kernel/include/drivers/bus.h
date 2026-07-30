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

typedef struct bus_addr {
	list_node_t node;
	list_t resources;
	device_t *device;
	char *name;
	bus_t *bus;
	int type;
} bus_addr_t;

#define BUS_PCI 1
#define BUS_PS2 2
#define BUS_USB 3

typedef struct bus_ops {
	ssize_t (*read)(bus_addr_t *addr, void *buf, off_t offset, size_t size);
	ssize_t (*write)(bus_addr_t *addr, const void *buf, off_t offset, size_t size);
	int (*old_register_handler)(bus_addr_t *addr, interrupt_handler_t handler, void *data);
	resource_t *(*allocate_resource)(bus_addr_t *addr, size_t start, size_t count, int flags, int rid);
	void (*release_resource)(resource_t *resource);
	int (*activate_resource)(resource_t *resource);
	void (*deactivate_resource)(resource_t *resource);
} bus_ops_t;

static inline int bus_activate_resource(bus_addr_t *addr, resource_t *resource) {
	if (!resource) {
		return -EINVAL;
	} else if ((resource->flags & RESOURCE_ACTIVE)) {
		return 0;
	} else if (!addr->bus->ops->activate_resource) {
		return -EINVAL;
	}
	int ret = addr->bus->ops->activate_resource(addr, resource);
	if (ret < 0) return ret;
	resource->flags |= RESOURCE_ACTIVE;
	return 0;
}

static inline void bus_deactivate_resource(bus_addr_t *addr, resource_t *resource) {
	if (!resource) return;
	if (!(resource->flags & RESOURCE_ACTIVE) || !addr->bus->ops->deactivate_resource) {
		return;
	}
	addr->bus->ops->deactivate_resource(addr, resource);
	resource->flags &= ~RESOURCE_ACTIVE;
}

static inline void bus_release_resource(bus_addr_t *addr, resource_t *resource) {
	if (!resource) return;
	if (resource->flags & RESOURCE_ACTIVE) {
		bus_deactivate_resource(addr, resource);
	}
	if (!addr->bus->ops->release_resource) {
		return;
	}
	addr->bus->ops->release_resource(addr, resource);
}

static inline resource_t *bus_allocate_resource(bus_addr_t *addr, size_t start, size_t count, int flags, int rid) {
	if (!addr->bus->ops->allocate_resource) {
		return ERR2PTR(-EINVAL);
	}
	resource_t *resource = addr->bus->ops->allocate_resource(addr, start, count, flags & ~RESOURCE_ACTIVE, rid);
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

static inline resource_t *bus_allocate_count_resource(bus_addr_t *addr, size_t count, int flags, int rid) {
	return bus_allocate_resource(addr, 0, count, flags, rid);
}

static inline resource_t *bus_allocate_simple_resource(bus_addr_t *addr, int flags, int rid) {
	return bus_allocate_resource(addr, 0, 0, flags, rid);
}

// TODO : remove this shit
static inline int bus_old_register_handler(bus_addr_t *addr, interrupt_handler_t handler, void *data) {
	if (!addr->bus->ops || !addr->bus->ops->old_register_handler) return -ENOTSUP;
	return addr->bus->ops->old_register_handler(addr, handler, data);
}

static inline ssize_t bus_read(bus_addr_t *addr, void *buf, off_t offset, size_t size) {
	if (!addr->bus->ops || !addr->bus->ops->read) return -ENOTSUP;
	return addr->bus->ops->read(addr, buf, offset, size);
}

static inline ssize_t bus_write(bus_addr_t *addr, const void *buf, off_t offset, size_t size) {
	if (!addr->bus->ops || !addr->bus->ops->write) return -ENOTSUP;
	return addr->bus->ops->write(addr, buf, offset, size);
}

#endif
