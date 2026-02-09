#include <kernel/hashmap.h>
#include <kernel/device.h>
#include <kernel/tmpfs.h>
#include <kernel/print.h>
#include <kernel/bus.h>
#include <errno.h>

hashmap_t device_drivers;
hashmap_t devices;
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
			destroy_device(addr->device);
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

	hashmap_foreach(major, driver, &device_drivers) {
		(void)major;
		ret = init_device_with_driver(addr, driver);
		return ret;
	}

	return ret;
}

int register_device_driver(device_driver_t *device_driver) {
	// default priority
	if (!device_driver->priority) device_driver->priority = 1;
	if (device_driver->major == 0) {
		// allocate a major
		static int major_dyn = 256;
		device_driver->major = major_dyn++;
	}
	if (hashmap_get(&device_drivers, device_driver->major)) return -EEXIST;
	hashmap_add(&device_drivers, device_driver->major, device_driver);

	// try to use this new driver on all already existing devices
	hashmap_foreach(number, device, &devices) {
		(void)number;
		bus_t *bus = (bus_t*)device;
		if (bus->device.type != DEVICE_BUS) continue;
		foreach (node, &bus->addresses) {
			bus_addr_t *addr = (bus_addr_t*)node;
			init_device_with_driver(addr, device_driver);
		}
	}
	return 0;
}

int unregister_device_driver(device_driver_t *device_driver) {
	hashmap_remove(&device_drivers, device_driver->major);
	return 0;
}

int register_device(device_t *device) {
	// TODO : create buses in devfs
	if (!device->number)  {
		device->number = device->driver->minor_count++;
	}
	device->number = makedev(device->driver->major, device->number);
	if (device->addr) {
		device->addr->device = device;
	}
	if (device->type == DEVICE_BUS) {
		vfs_createat(devfs_root, device->name, 0666, VFS_DIR);
	} else {
		vfs_createat_ext(devfs_root, device->name, 0666, device->type == DEVICE_CHAR ? VFS_CHAR : VFS_BLOCK, &device->number);
	}
	kdebugf("register device %s as %d,%d (%lx)\n", device->name, major(device->number), minor(device->number), device->number);
	hashmap_add(&devices, device->number, device);
	if (device->type == DEVICE_BUS) {
		bus_t *bus = (bus_t*)device;
		foreach (node, &bus->addresses) {
			bus_addr_t *addr = (bus_addr_t*)node;
			// just in case the driver forgot
			addr->bus = bus;
			init_device(addr);
		}
	}
	return 0;
}

int destroy_device(device_t *device) {
	hashmap_remove(&devices, device->number);
	device->type = DEVICE_UNPLUGED;
	if (device->destroy) device->destroy(device);
	// TODO : remove in devfs
	return 0;
}

device_t *device_from_number(dev_t dev) {
	return hashmap_get(&devices, dev);
}

vfs_fd_t *open_device(device_t *device, long flags) {
	vfs_fd_t *fd = kmalloc(sizeof(vfs_fd_t));
	memset(fd, 0, sizeof(vfs_fd_t));
	fd->ops = device->ops;
	fd->type = device->type == DEVICE_BLOCK ? VFS_BLOCK : VFS_CHAR;
	fd->flags = flags;
	fd->ref_count = 1;
	fd->private = device;
	if (fd->ops->open) {
		if (fd->ops->open(fd) < 0) {
			kfree(fd);
			return NULL;
		}
	}
	return fd;
}

void init_devices(void) {
	kstatusf("init devices ... ");
	init_hashmap(&devices, 256);
	init_hashmap(&device_drivers, 256);

	vfs_superblock_t *devfs_superblock = new_tmpfs();
	vfs_mount("/dev", devfs_superblock);
	devfs_root = vfs_get_dentry("/dev", 0);
	
	kok();
}
