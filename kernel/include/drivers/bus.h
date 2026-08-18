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
	list_t resources;
	list_t resource_descs;
	list_t children;
	struct devnode *parent;
	struct device *device;
	struct driver *driver;
	void *private;
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
	resource_t *(*allocate_resource)(devnode_t *bus, devnode_t *devnode, resource_request_t *request, int rid);
	int (*release_resource)(devnode_t *bus, devnode_t *devnode, resource_t *resource);
	int (*activate_resource)(devnode_t *bus, devnode_t *devnode, resource_t *resource);
	int (*deactivate_resource)(devnode_t *bus, devnode_t *devnode, resource_t *resource);
} driver_t;

#define BUSES(...) (const char *[]){__VA_ARGS__, NULL}

#define BUS_PCI 1
#define BUS_PS2 2
#define BUS_USB 3
#define BUS_ISA 4

int driver_register(driver_t *driver);
int driver_unregister(driver_t *driver);

devnode_t *bus_attach_child(devnode_t *bus, devnode_t *child, const char *name, int unit);
int bus_delete_child(devnode_t *bus, devnode_t *child);

devnode_t *device_allocate(void);
int device_check_driver(devnode_t *device, driver_t *driver);
int device_attach_driver(devnode_t *device, driver_t *driver);
int device_attach_driver_auto(devnode_t *device);
int device_detach_driver(devnode_t *device);
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

static inline void bus_attach_resource_desc(devnode_t *devnode, resource_desc_t *resource_desc) {
	list_append(&devnode->resource_descs, &resource_desc->node);
}

static inline int bus_add_resource_desct(devnode_t *devnode, resource_request_t *request, int rid) {
	resource_desc_t *desc = resource_desc_allocate(request, rid);
	if (!desc) return -ENOMEM;
	bus_attach_resource_desc(devnode, desc);
	return 0;
}

static inline int bus_add_fixed_resource_desc(devnode_t *devnode, size_t start, size_t size, int flags, int rid) {
	resource_request_t request = {
		.start = start,
		.end   = start + size,
		.size  = size,
		.flags = flags,
	};
	return bus_add_resource_desc_request(devnode, &request, rid);
}

static inline int bus_add_resource_desc_data(devnode_t *devnode, void *data, size_t size, int flags, int rid) {
	return bus_add_fixed_resource_desc(devnode, (size_t)data, size, flags, rid);
}

static inline void bus_detach_resource_desc(devnode_t *devnode, resource_desc_t *resource_desc) {
	list_remove(&devnode->resource_descs, &resource_desc->node);
}

static inline resource_desc_t *device_get_resource_desc(devnode_t *devnode, int flags, int rid) {
	if (rid == RID_NONE) return NULL;
	foreach (node, &devnode->resource_descs) {
		resource_desc_t *desc = container_of(node, resource_desc_t, node);
		if ((desc->request.flags & RESOURCE_TYPE) == (flags & RESOURCE_TYPE) && ((rid == RID_ANY) || (desc->rid == rid))) {
			return desc;
		}
	}
	return NULL;
}

static inline resource_t *device_get_resource(devnode_t *devnode, int flags, int rid) {
	if (rid == RID_NONE) return NULL;
	foreach (node, &devnode->resources) {
		resource_t *resource = container_of(node, resource_t, node);
		if ((resource->flags & RESOURCE_TYPE) == (flags & RESOURCE_TYPE) && ((rid == RID_ANY) || (resource->rid == rid))) {
			return resource;
		}
	}
	return NULL;
}

resource_t *bus_allocate_resource(devnode_t *bus, devnode_t *devnode, resource_request_t *request, int rid);

static inline resource_t *bus_allocate_fixed_resource(devnode_t *bus, devnode_t *devnode, size_t start, size_t size, int flags, int rid) {
	resource_request_t request = {
		.start = start,
		.end   = start + size,
		.size  = size,
		.flags = flags,
	};
	return bus_allocate_resource(bus, devnode, &request, rid);
}

static inline resource_t *bus_allocate_size_resource(devnode_t *bus, devnode_t *devnode, size_t size, int flags, int rid) {
	resource_request_t request = {
		.start = RESOURCE_ANY_START,
		.end   = RESOURCE_ANY_END,
		.size  = size,
		.flags = flags,
	};
	return bus_allocate_resource(bus, devnode, &request, rid);
}

static inline resource_t *bus_allocate_simple_resource(devnode_t *bus, devnode_t *devnode, int flags, int rid) {
	return bus_allocate_size_resource(bus, devnode, RESOURCE_ANY_SIZE, flags, rid);
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

int bus_release_resource(devnode_t *bus, devnode_t *devnode, resource_t *resource);

static inline resource_t *device_allocate_resource(devnode_t *devnode, resource_request_t *request, int rid) {
	return bus_allocate_resource(devnode->parent, devnode, request, rid);
}

static inline resource_t *device_allocate_fixed_resource(devnode_t *devnode, size_t start, size_t size, int flags, int rid) {
	resource_request_t request = {
		.start = start,
		.end   = start + size,
		.size  = size,
		.flags = flags,
	};
	return device_allocate_resource(devnode, &request, rid);
}

static inline resource_t *device_allocate_size_resource(devnode_t *devnode, size_t size, int flags, int rid) {
	resource_request_t request = {
		.start = RESOURCE_ANY_START,
		.end   = RESOURCE_ANY_END,
		.size  = size,
		.flags = flags,
	};
	return device_allocate_resource(devnode, &request, rid);
}

static inline resource_t *device_allocate_simple_resource(devnode_t *devnode, int flags, int rid) {
	return device_allocate_size_resource(devnode, RESOURCE_ANY_SIZE, flags, rid);
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
