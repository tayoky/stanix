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
	device_driver_t *driver;
	struct bus *bus;
	char *name;
	vfs_ops_t *ops;
	int number;
	int type;
} device_t;

#define DEVICE_CHAR  1
#define DEVICE_BLOCK 2
#define DEVICE_BUS   3

int register_device_driver(device_driver_t *device_driver);
int unregister_device_driver(device_driver_t *device_driver);

/**
 * @brief register a new device
 * @param device the device to register
 * @note if number is 0 a dev number is automaticly allocated
 */
int register_device(device_t *device);

/**
 * @brief destroy a device
 * @param device the device to destroy
 */
int destroy_device(device_t *device);

/**
 * @brief get a device from its dev number
 * @param dev the dev number to find the device from
 */
device_t *device_from_number(dev_t dev);


void init_devices(void);

#endif
