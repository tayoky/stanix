#include <libutils/hashmap.h>
#include <kernel/device.h>
#include <kernel/tmpfs.h>
#include <kernel/print.h>
#include <kernel/bus.h>
#include <errno.h>

utils_hashmap_t device_drivers;
utils_hashmap_t devices;
vfs_node_t *devfs_root;

static int init_device_with_driver(bus_addr_t *addr, device_driver_t *device_driver) {	
	if (!device_driver->check || !device_driver->probe) return -ENOTSUP;
	if (!device_driver->check(addr)) return -ENOTSUP;

	if (addr->device) {
		// a driver already control this address
		if (addr->device->driver->priority > driver->priority) {
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

struct __init_device_struct_helper {
	bus_addr_t *addr;
	int ret;
};

static void __init_device_helper(void *element, void *arg) {
	struct __init_device_struct_helper *data = arg;
	device_driver_t *driver = element;

	if (data->ret >= 0) return;
	data->ret = init_device_with_driver(data->addr, driver);
}

static int init_device(bus_addr_t *addr) {
	if (addr->device) {
		// a driver already control this address
		return -EBUSY;
	}
	
	struct __init_device_struct_helper data = {
		.addr = addr,
		.ret = -ENOTSUP,
	};

	utils_hashmap_foreach(&device_drivers, __init_device_helper, &data); 

	return data.ret;
}

static void __init_bus_with_driver_helper(void *element, void *arg) {
	bus_t *bus = element;
	device_driver_t *driver = arg;
	if (bus->device.type != DEVICE_BUS) return;
	foreach (node, bus->addresses) {
		bus_addr_t *addr = node->value;
		init_device_with_driver(addr, driver);
	}
}

int register_device_driver(device_driver_t *device_driver) {
	// default priority
	if (!driver->priority) driver->priority = 1;
	if (device_driver->major == 0) {
		// allocate a major
		static int major_dyn = 256;
		device_driver->major = major_dyn++;
	}
	if (utils_hashmap_get(&device_drivers, device_driver->major)) return -EEXIST;
	utils_hashmap_add(&device_drivers, device_driver->major, device_driver);

	// try to use this new driver on all already existing devices
	utils_hashmap_foreach(&devices, __init_bus_with_driver_helper, device_driver);
	return 0;
}

int unregister_device_driver(device_driver_t *device_driver) {
	utils_hashmap_remove(&device_drivers, device_driver->major);
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
	utils_hashmap_add(&devices, device->number, device);
	if (device->type == DEVICE_BUS) {
		bus_t *bus = (bus_t*)device;
		foreach (node, bus->addresses) {
			bus_addr_t *addr = node->value;
			init_device(addr);
		}
	}
	return 0;
}

int destroy_device(device_t *device) {
	device->type = DEVICE_UNPLUGED;
	utils_hashmap_remove(&devices, device->number);
	if (device->cleanup) device->cleanup(device);
	// TODO : remove in devfs
	return 0;
}

device_t *device_from_number(dev_t dev) {
	return utils_hashmap_get(&devices, dev);
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
	utils_init_hashmap(&devices, 256);
	utils_init_hashmap(&device_drivers, 256);
	devfs_root = new_tmpfs();
	vfs_mount("/dev", devfs_root);
	kok();
}
