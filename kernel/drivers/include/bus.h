#ifndef _KERNEL_BUS_H
#define _KERNEL_BUS_H

#include <kernel/device.h>
#include <kernel/list.h>

typedef struct bus {
	device_t device;
	list_t *addresses;
} bus_t;

typedef struct bus_addr {
	vfs_node node;
	device_t *device;
	int type;
} bus_addr_t;

#define BUS_PCI 1
#define BUS_PS2 2
#define BUS_USB 3

#endif
