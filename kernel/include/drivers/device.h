#ifndef KERNEL_DEVICE_H
#define KERNEL_DEVICE_H

#include <kernel/vfs.h>
#include <kernel/xarray.h>
#include <kernel/refcount.h>
#include <sys/sysmacros.h>

struct bus;
struct devnode;
struct device;

typedef struct device {
	ref_count_t ref_count;
	struct devnode *devnode;
	void (*destroy)(struct device *);
	void (*cleanup)(struct device *);
	char *name;
	vfs_fd_ops_t *ops;
	dev_t number;
	int type;
} device_t;

#define DEVICE_CHAR      1
#define DEVICE_BLOCK     2
#define DEVICE_UNPLUGGED 3

// dynamic major number management
int device_allocate_major(void);
void device_set_major(int major);
void device_free_major(int major);

/**
 * @brief register a new device
 * @param device the device to register
 * @param fmt a format for the name of the device
 * @param number device number
 * @note if major or minor are 0 they are automatically allocated
 */
int device_register(device_t *device, const char *fmt, dev_t number);

/**
 * @brief destroy a device
 * @param device the device to destroy
 */
int device_destroy(device_t *device);

/**
 * @brief get a device from its dev number
 * @param dev the dev number to find the device from,
 * @return the device and create a ref that must be released using \ref device_release
 */
device_t *device_from_number(dev_t dev);

/**
 * @brief get a device from its name
 * @param name the name of the device
 * @return a new ref to the device that must be released with \ref device_release
 */
device_t *device_from_name(const char *name);

/**
 * @brief open a device without going through a path
 * @param device the device to open
 * @param flags flags to open the device with
 * @return a vfs file descriptor to the device or NULL on error
 */
vfs_fd_t *device_open(device_t *device, long flags);

/**
 * @brief create a new reference to a device
 * @param device the device to create a ref to
 * @return the device
 */
static inline device_t *device_ref(device_t *device) {
	if (device) ref_count_inc(&device->ref_count);
	return device;
}

/**
 * @brief release a reference to a device
 * @param device the device to release
 */
void device_release(device_t *device);

static inline int device_is_unplugged(device_t *device) {
	return device->type == DEVICE_UNPLUGGED;
}

void init_devices(void);
void init_mem_devices(void);

extern xarray_t devices;

#endif
