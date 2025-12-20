#ifndef _KERNEL_DEVICE_H
#define _KERNEL_DEVICE_H

#include <kernel/vfs.h>

struct bus;
struct bus_addr;

typedef struct device_driver {
	const char *name;
	int (*check)(struct bus_addr *addr);
	int (*init_device)(struct bus_addr *addr);
	int major; // all devices that use this driver have this
} device_driver_t;

typedef struct device {
	vfs_node node;
	device_driver_t *driver;
	struct bus *bus;
	char *name;
	int number;
	int type;
} device_t;

#define DEVICE_CHAR  1
#define DEVICE_BLOCK 2
#define DEVICE_BUS   3

int register_device_driver(device_driver_t *device_driver);
int unregister_device_driver(device_driver_t *device_driver);
int register_device(device_t *device);
int unregister_device(device_t *device);

#endif
