#ifndef _KERNEL_DEVICE_H
#define _KERNEL_DEVICE_H

#include <kernel/vfs.h>
#include <sys/sysmacros.h>
#include <stdatomic.h>

struct bus;
struct bus_addr;
struct device;

typedef struct device_driver {
	const char *name;
	int (*check)(struct bus_addr *addr);
	int (*probe)(struct bus_addr *addr);
	int major; // all devices that use this driver have this
	int minor_count;
	int priority;
} device_driver_t;

typedef struct device {
	atomic_size_t ref_count;
	device_driver_t *driver;
	struct bus_addr *addr;
	void (*destroy)(struct device *);
	void (*cleanup)(struct device *);
	char *name;
	vfs_fd_ops_t *ops;
	dev_t number;
	int type;
} device_t;

#define DEVICE_CHAR     1
#define DEVICE_BLOCK    2
#define DEVICE_BUS      3
#define DEVICE_UNPLUGGED 4

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
 * @param dev the dev number to find the device from, 
 * @return the device and create a ref that must be released using \ref device_release
 */
device_t *device_from_number(dev_t dev);

/**
 * @brief open a device without going through a path
 * @param device the device to open
 * @param flags flags to open the device with
 * @return a vfs file descriptor to the device or NULL on error
 */
vfs_fd_t *open_device(device_t *device, long flags);

/**
 * @brief create a new reference to a device
 * @param device the device to create a ref to
 * @return the device
 */
static inline device_t *device_ref(device_t *device) {
	if (device) atomic_fetch_add(&device->ref_count, 1);
	return device;
}

/**
 * @brief release a reference to a device
 * @param device the device to release
 */
void device_release(device_t *device);

static inline device_is_unplugged(device_t *device) {
	return device->type == DEVICE_UNPLUGGED;
}

void init_devices(void);
void init_mem_devices(void);

extern hashmap_t devices;

#endif
