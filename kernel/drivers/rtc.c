#include <kernel/rtc.h>
#include <kernel/userspace.h>
#include <kernel/spinlock.h>
#include <kernel/atomic.h>
#include <kernel/slab.h>

static slab_cache_t rtc_devices_slab;
static spinlock_t rtcs_lock;
static list_t rtcs ATTR_GUARDED_BY(rtcs_lock);
static ATOMIC(rtc_device_t *) main_rtc;

void init_rtc(void) {
	slab_init(&rtc_devices_slab, sizeof(rtc_device_t), "rtc-devices");
}

static int rtc_autoselect(void) ATTR_REQUIRE(&rtcs_lock) {
	int best_priority = -1;
	rtc_device_t *best_rtc = NULL;
	foreach (node, &rtcs) {
		rtc_device_t *rtc_device = container_of(node, rtc_device_t, node);
		if (rtc_device->priority > best_priority) {
			best_priority = rtc_device->priority;
			best_rtc = rtc_device;
		}
	}
	if (main_rtc == best_rtc) {
		return 0;
	}
	atomic_store(&main_rtc, best_rtc);
	return 1;
}

static ssize_t rtc_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t size) {
	(void)offset;
	rtc_device_t *rtc_device = container_of(fd->private, rtc_device_t, device);
	if (size != sizeof(struct timespec)) return -EINVAL;
	
	struct timespec time;
	int ret = rtc_device_get_time(rtc_device, &time);
	if (ret < 0) return ret;

	return safe_copy_auto_to(buffer, &time);
}

static ssize_t rtc_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t size) {
	(void)offset;
	rtc_device_t *rtc_device = container_of(fd->private, rtc_device_t, device);
	if (size != sizeof(struct timespec)) return -EINVAL;

	struct timespec time;
	int ret = safe_copy_auto_from(&time, buffer);
	if (ret < 0) return ret;

	return rtc_device_set_time(rtc_device, &time);
}

static vfs_fd_ops_t rtc_ops = {
	.read  = rtc_read,
	.write = rtc_write,
};

static void rtc_destroy(device_t *device) {
	rtc_device_t *rtc_device = container_of(device, rtc_device_t, device);
	spinlock_acquire(&rtcs_lock);
	atomic_store(&rtc_device->unplugged, 1);
	list_remove(&rtcs, &rtc_device->node);
	if (rtc_get_main() == rtc_device) {
		rtc_autoselect();
	}
	spinlock_release(&rtcs_lock);
}

static void rtc_cleanup(device_t *device) {
	rtc_device_t *rtc_device = container_of(device, rtc_device_t, device);
	slab_free(rtc_device);
}

rtc_device_t *rtc_device_allocate(void) {
	rtc_device_t *rtc_device = slab_alloc(&rtc_devices_slab);
	if (!rtc_device) return NULL;
	memset(rtc_device, 0, sizeof(rtc_device_t));
	return rtc_device;
}

int rtc_device_register(rtc_device_t *rtc_device, const char *fmt, dev_t number) {
	rtc_device->device.type    = DEVICE_CHAR;
	rtc_device->device.ops     = &rtc_ops;
	rtc_device->device.destroy = rtc_destroy;
	rtc_device->device.cleanup = rtc_cleanup;

	int ret = device_register(&rtc_device->device, fmt, number);
	if (ret < 0) return ret;

	spinlock_acquire(&rtcs_lock);
	list_append(&rtcs, &rtc_device->node);
	ret = rtc_autoselect();
	spinlock_release(&rtcs_lock);
	if (ret) {
		// TODO : recallibrate clocks
	}
	return 0;
}

rtc_device_t *rtc_get_main(void) {
	return atomic_load(&main_rtc);
}
