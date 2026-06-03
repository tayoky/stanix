#include <kernel/xarray.h>
#include <kernel/device.h>
#include <kernel/tmpfs.h>
#include <kernel/print.h>
#include <kernel/slab.h>
#include <kernel/bus.h>
#include <errno.h>

xarray_t device_drivers;
xarray_t devices;
vfs_dentry_t *devfs_root;

static int init_device_with_driver(bus_addr_t *addr, device_driver_t *device_driver) {
	if (!device_driver->check || !device_driver->probe) return -ENOTSUP;
	if (!device_driver->check(addr)) return -ENOTSUP;

	if (addr->device) {
		// a driver already control this address
		if (addr->device->driver->priority > device_driver->priority) {
			// the driver is already better
			return -EBUSY;
		} else {
			// replace the old driver
			device_destroy(addr->device);
		}
	}

	// the driver is compatible with the device
	return device_driver->probe(addr);
}

static int init_device(bus_addr_t *addr) {
	if (addr->device) {
		// a driver already control this address
		return -EBUSY;
	}

	int ret = -ENOTSUP;

	xarray_foreach (major, driver, &device_drivers) {
		(void)major;
		ret = init_device_with_driver(addr, driver);
		return ret;
	}

	return ret;
}

int device_driver_register(device_driver_t *device_driver) {
	// default priority
	if (!device_driver->priority) device_driver->priority = 1;
	if (device_driver->major == 0) {
		// allocate a major
		// dynamic majors start at 256
		device_driver->major = xarray_allocate_from(&device_drivers, 256, device_driver);
	} else {
		if (xarray_get(&device_drivers, device_driver->major)) return -EEXIST;
		xarray_set(&device_drivers, device_driver->major, device_driver);
	}

	// try to use this new driver on all already existing devices
	xarray_foreach (number, device, &devices) {
		(void)number;
		bus_t *bus = (bus_t *)device;
		if (bus->device.type != DEVICE_BUS) continue;
		foreach (node, &bus->addresses) {
			bus_addr_t *addr = (bus_addr_t *)node;
			init_device_with_driver(addr, device_driver);
		}
	}
	return 0;
}

int device_driver_unregister(device_driver_t *device_driver) {
	xarray_clear(&device_drivers, device_driver->major);
	return 0;
}

int device_register_fmt(device_t *device, const char *fmt) {
	if (device->addr) {
		device->addr->device = device;
	}
	device->ref_count = 1;
	if (!device->number) {
		device->number = xarray_allocate_from(&devices, makedev(device->driver->major, 0), device);
	} else {
		device->number = makedev(device->driver->major, device->number);
		xarray_set(&devices, device->number, device);
	}
	if (fmt) {
		char name[256];
		snprintf(name, sizeof(name), fmt, minor(device->number));
		device->name = strdup(name);
	}
	if (device->type != DEVICE_BUS) {
		vfs_mknod_at(devfs_root, device->name, 0666 | (device->type == DEVICE_CHAR ? S_IFCHR : S_IFBLK), device->number);
	}
	kdebugf("register device %s as %d,%d (%lx)\n", device->name, major(device->number), minor(device->number), device->number);
	if (device->type == DEVICE_BUS) {
		bus_t *bus = (bus_t *)device;
		foreach(node, &bus->addresses) {
			bus_addr_t *addr = (bus_addr_t *)node;
			// just in case the driver forgot
			addr->bus = bus;
			init_device(addr);
		}
	}
	return 0;
}

int device_register(device_t *device) {
	return device_register_fmt(device, NULL);
}

void device_release(device_t *device) {
	if (device->ref_count > 1) {
		device->ref_count--;
		return;
	}
	if (device->cleanup) device->cleanup(device);
}

int device_destroy(device_t *device) {
	xarray_clear(&devices, device->number);
	device->type = DEVICE_UNPLUGGED;
	if (device->destroy) device->destroy(device);
	vfs_unlink_at(devfs_root, device->name);
	device_release(device);
	return 0;
}

device_t *device_from_number(dev_t dev) {
	rcu_acquire_read(&devices.rcu);
	device_t *device = xarray_get(&devices, dev);
	device_ref(device);
	rcu_release_read(&devices.rcu);
	return device;
}

vfs_fd_t *device_open(device_t *device, long flags) {
	vfs_fd_t *fd = vfs_alloc_fd();
	fd->ops = device->ops;
	fd->type = device->type == DEVICE_BLOCK ? VFS_BLOCK : VFS_CHAR;
	fd->flags = flags;
	fd->private = device_ref(device);
	if (fd->ops->open) {
		if (fd->ops->open(fd) < 0) {
			slab_free(fd);
			return NULL;
		}
	}
	return fd;
}

void init_devices(void) {
	kstatusf("init devices ... ");
	xarray_init(&devices);
	xarray_init(&device_drivers);

	vfs_superblock_t *devfs_superblock = new_tmpfs();
	vfs_mount("/dev", devfs_superblock);
	devfs_root = vfs_get_dentry("/dev", 0);

	kok();
}
