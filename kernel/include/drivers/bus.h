#ifndef _KERNEL_BUS_H
#define _KERNEL_BUS_H

#include <kernel/device.h>
#include <kernel/interrupt.h>
#include <kernel/list.h>
#include <sys/types.h>
#include <errno.h>

struct bus;
struct bus_addr;

typedef struct bus_ops {
	ssize_t (*read)(struct bus_addr *addr, void *buf, off_t offset, size_t size);
	ssize_t (*write)(struct bus_addr *addr, const void *buf, off_t offset, size_t size);
	int (*register_handler)(struct bus_addr *addr, interrupt_handler_t handler, void *data);
} bus_ops_t;

typedef struct bus {
	device_t device;
	list_t addresses;
	bus_ops_t *ops;
} bus_t;

typedef struct bus_addr {
	list_node_t node;
	device_t *device;
	char *name;
	bus_t *bus;
	int type;
} bus_addr_t;

#define BUS_PCI 1
#define BUS_PS2 2
#define BUS_USB 3

static inline int bus_register_handler(bus_addr_t *addr, interrupt_handler_t handler, void *data) {
	if (!addr->bus->ops || !addr->bus->ops->register_handler) return -ENOTSUP;
	return addr->bus->ops->register_handler(addr, handler, data);
}

static inline bus_read(bus_addr_t *addr, void *buf, off_t offset, size_t size) {
	if (!addr->bus->ops || !addr->bus->ops->read) return -ENOTSUP;
	return addr->bus->ops->read(addr, buf, offset, size);
}

static inline bus_write(bus_addr_t *addr, const void *buf, off_t offset, size_t size) {
	if (!addr->bus->ops || !addr->bus->ops->write) return -ENOTSUP;
	return addr->bus->ops->write(addr, buf, offset, size);
}

#endif
