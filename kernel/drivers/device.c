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
	if (addr->device) {
		// a driver already control this address
		return -EBUSY;
	}
	
	if (!device_driver->check || !device_driver->init_device) return -ENOTSUP;
	if (!device_driver->check(addr)) return -ENOTSUP;
	
	// the driver is compatible with the device
	return device_driver->init_device(addr);
}

static int init_device(bus_addr_t *addr) {
	if (addr->device) {
		// a driver already control this address
		return -EBUSY;
	}

	int ret = -ENOTSUP;

	/*utils_hashmap_foreach(&device_drivers, ); {
		ret = udrv_init_device_with_typedef(addr, device_driver);
		if (ret < 0) continue;

		// the driver is compatible
		break;
	}*/

	return ret;
}

static void init_bus_with_driver_helper(void *element, void *arg) {
	bus_t *bus = element;
	device_driver_t *driver = arg;
	if (bus->device.type != DEVICE_BUS) return;
	foreach (node, bus->addresses) {
		bus_addr_t *addr = node->value;
		init_device_with_driver(addr, driver);
	}
}

int register_device_driver(device_driver_t *device_driver) {
	if (device_driver->major == 0) {
		// allocate a major
		static int major_dyn = 256;
		device_driver->major = major_dyn++;
	}
	if (utils_hashmap_get(&device_drivers, device_driver->major)) return -EEXIST;
	utils_hashmap_add(&device_drivers, device_driver->major, device_driver);

	// try to use this new driver on all already existing devices
	utils_hashmap_foreach(&devices, init_bus_with_driver_helper, device_driver);
	return 0;
}

int unregister_device_driver(device_driver_t *device_driver) {
	utils_hashmap_remove(&device_drivers, device_driver->major);
	return 0;
}

int register_device(device_t *device) {
	// TODO : allocate dev number
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
	utils_hashmap_remove(&devices, device->number);
	return 0;
}

device_t *device_from_number(dev_t dev) {
	return utils_hashmap_get(&devices, dev);
}

void init_devices(void) {
	kstatusf("init devices ... ");
	utils_init_hashmap(&devices, 256);
	utils_init_hashmap(&device_drivers, 256);
	devfs_root = new_tmpfs();
	vfs_mount("/dev", devfs_root);
	kok();
}
