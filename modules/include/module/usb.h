#ifndef MODULE_USB_H
#define MODULE_USB_H

#include <kernel/bus.h>

typedef struct usb_device_info {
	uint16_t vendor_id;
	uint16_t product_id;
	uint8_t class;
	uint8_t sub_class;
	uint8_t speed;
} usb_device_info_t;

typedef struct usb_addr {
	bus_addr_t addr;
	usb_device_info_t info;
	uint8_t port; // port on the bus of the device
} usb_addr_t;

typedef struct usb_endpoint {
	int type;
	int direction;
	int number;
	size_t max_packet_size;
} usb_endpoint_t;

#define USB_TRANSFER_TYPE_CONTROL     0
#define USB_TRANSFER_TYPE_ISOCHRONOUS 0
#define USB_TRANSFER_TYPE_BULK        0
#define USB_TRANSFER_TYPE_INTERRUPT   0

#define USB_DIRECTION_IN  0
#define USB_DIRECTION_OUT 1

#define USB_ENDPOINT_CONTROL 0

typedef struct usb_bus_ops {
	bus_ops_t bus_ops;
} usb_bus_ops_t;

usb_endpoint_t *usb_endpoint_setup(usb_addr_t *addr, int number, int direction);
void usb_endpoint_close(usb_addr_t *addr, usb_endpoint_t *endpoint);
int usb_control_transfer(usb_addr_t *addr, usb_endpoint_t *endpoint, int request_type, int request, int value, int index, void *data, size_t size);
ssize_t usb_bulk_transfer(usb_addr_t *addr, usb_endpoint_t *endpoint, void *data, size_t size);
ssize_t usb_interrupt_transfer(usb_addr_t *addr, usb_endpoint_t *endpoint, void *data, size_t size);

#endif
