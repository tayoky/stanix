#include <kernel/xarray.h>
#include <kernel/device.h>
#include <kernel/tmpfs.h>
#include <kernel/print.h>
#include <kernel/slab.h>
#include <kernel/devclass.h>
#include <kernel/bus.h>
#include <errno.h>

#define DYNAMIC_MAJOR_MIN 256
#define DYNAMIC_MAJOR_MAX 512
#define DYNAMIC_MAJOR_SIZE (DYNAMIC_MAJOR_MAX - DYNAMIC_MAJOR_MIN)
static uint32_t major_bitmap[DYNAMIC_MAJOR_SIZE / 32];
xarray_t devices;
vfs_dentry_t *devfs_root;

int device_allocate_major(void) {
	for (size_t i=0; i<arraylen(major_bitmap); i++) {
		if (major_bitmap[i] == 0xffffffffU) {
			continue;
		}
		for (size_t j=0; j<32; j++) {
			if (!(major_bitmap[i] & (1U << j))) {
				major_bitmap[i] |= (1U << j);
				return DYNAMIC_MAJOR_MIN + i * 32 + j;
			}
		}
		i++;
	}
	return -1;
}

void device_set_major(int major) {
	if (major < DYNAMIC_MAJOR_MIN || major >= DYNAMIC_MAJOR_MAX) {
		return;
	}
	major -= DYNAMIC_MAJOR_MIN;
	major_bitmap[major / 32] |= (1U << (major % 32));
}

void device_free_major(int major) {
	if (major < DYNAMIC_MAJOR_MIN || major >= DYNAMIC_MAJOR_MAX) {
		return;
	}
	major -= DYNAMIC_MAJOR_MIN;
	major_bitmap[major / 32] &= ~(1U << (major % 32));
}

int device_register(device_t *device, const char *fmt, dev_t number) {
	if (device->devnode) {
		device->devnode->device = device;
	}
	device->ref_count = 1;

	// allocate numbers if required
	if (major(number) == 0) {
		int major = device_allocate_major();
		number = makedev(major, minor(number));
	}
	if (minor(number) == 0) {
		if (device->devnode && (int)major(number) == device->devnode->devclass->major) {
			int minor = device->devnode->unit;
			number = makedev(major(number), minor);
		} else {
			// automatically allocate number
			number = xarray_allocate_from(&devices, number, device);
		}
	}

	if (fmt) {
		char name[256];
		snprintf(name, sizeof(name), fmt, minor(number));
		device->name = strdup(name);
	} else if (device->devnode) {
		// take the name direcly from the bus subsystem
		device->name = device_get_dup_name(device->devnode);
	}

	device->number = number;
	xarray_set(&devices, number, device);

	if (device->name) {
		vfs_mknod_at(devfs_root, device->name, 0666 | (device->type == DEVICE_CHAR ? S_IFCHR : S_IFBLK), number);
	}
	kdebugf("register device %s as %d,%d (%lx)\n", device->name, major(device->number), minor(device->number), device->number);
	return 0;
}

void device_release(device_t *device) {
	if (ref_count_dec(&device->ref_count) > 1) {
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
	vfs_fd_t *fd = vfs_fd_alloc();
	fd->ops = device->ops;
	fd->type = device->type == DEVICE_BLOCK ? S_IFBLK : S_IFCHR;
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

	vfs_superblock_t *devfs_superblock = new_tmpfs();
	vfs_mount("/dev", 0, devfs_superblock);
	devfs_root = vfs_get_dentry("/dev", 0);

	kok();
}
