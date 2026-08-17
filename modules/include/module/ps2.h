#ifndef PS2_H
#define PS2_H

#include <kernel/bus.h>
#include <stdint.h>

// devices commands
#define PS2_ECHO             0xEE
#define PS2_IDENTIFY         0xF2
#define PS2_ENABLE_SCANNING  0xF4
#define PS2_DISABLE_SCANNING 0xF5
#define PS2_RESET            0xFF

#define PS2_ACK               0xFA
#define PS2_RESEND            0xFE
#define PS2_SELF_TEST_PASSED  0xAA
#define PS2_SELF_TEST_FAILED1 0xFC
#define PS2_SELF_TEST_FAILED2 0xFD

#define PS2_RID_IRQ 1

typedef int ps2_device_id_t[2];
typedef struct ps2_driver ps2_driver_t;
typedef struct ps2_dev ps2_dev_t;

struct ps2_driver {
	driver_t driver;
	int (*send)(devnode_t *controller, ps2_dev_t *device, uint8_t data);
	int (*read)(devnode_t *controller, ps2_dev_t *device);
};

struct ps2_dev {
	devnode_t devnode;
	devnode_t *controller;
	ps2_device_id_t device_id;
};

int ps2_send(ps2_dev_t *device, uint8_t data) {
	ps2_driver_t *ps2_driver = container_of(device->controller->driver, ps2_driver_t, driver);
	if (ps2_driver->send) {
		return ps2_driver->send(device->controller, device, data);
	}
	return -ENOTSUP;
}

static inline int ps2_read(ps2_dev_t *device) {
	ps2_driver_t *ps2_driver = container_of(device->controller->driver, ps2_driver_t, driver);
	if (ps2_driver->read) {
		return ps2_driver->read(device->controller, device);
	}
	return -ENOTSUP;
}

static inline int ps2_send_command(ps2_dev_t *device, uint8_t data) {
	int ret = ps2_send(device, data);
	if (ret < 0) return ret;
	return ps2_read(device);
}

int ps2_add_device(devnode_t *controller, ps2_dev_t *device);
int ps2_reset(ps2_dev_t *device);

#endif
