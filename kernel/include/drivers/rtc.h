#ifndef KERNEL_RTC_H
#define KERNEL_RTC_H

#include <kernel/time.h>
#include <kernel/device.h>
#include <kernel/list.h>
#include <kernel/attributes.h>

typedef struct rtc_ops rtc_ops_t;
typedef struct rtc_device rtc_device_t;

struct rtc_ops {
	int (*get_time)(rtc_device_t *rtc_device, struct timespec *time);
	int (*set_time)(rtc_device_t *rtc_device, const struct timespec *time);
	void (*cleanup)(rtc_device_t *rtc_device);
};

struct rtc_device {
	device_t device;
	list_node_t node;
	rtc_ops_t *ops;
	void *private;
	ATOMIC(int) unplugged; // write protectes by rtcs_lock
	int priority;
};

ATTR_MUST_USE rtc_device_t *rtc_device_allocate(void);
ATTR_MUST_USE int rtc_device_register(rtc_device_t *rtc_device, const char *fmt, dev_t number);

static inline int rtc_device_get_time(rtc_device_t *rtc_device, struct timespec *time) {
	if (rtc_device->ops && rtc_device->ops->get_time) {
		return rtc_device->ops->get_time(rtc_device, time);
	} else {
		return -ENOTSUP;
	}
}

static inline int rtc_device_set_time(rtc_device_t *rtc_device, const struct timespec *time) {
	if (rtc_device->ops && rtc_device->ops->set_time) {
		return rtc_device->ops->set_time(rtc_device, time);
	} else {
		return -ENOTSUP;
	}
}

rtc_device_t *rtc_get_main(void);

static inline int rtc_get_time(struct timespec *timespec) {
	rtc_device_t *main_rtc = rtc_get_main();
	if (!main_rtc) return -ENXIO;
	return rtc_device_get_time(main_rtc, timespec);
}

static inline int rtc_set_time(const struct timespec *timespec) {
	rtc_device_t *main_rtc = rtc_get_main();
	if (!main_rtc) return -ENXIO;
	return rtc_device_set_time(main_rtc, timespec);
}

void init_rtc(void);

#endif
