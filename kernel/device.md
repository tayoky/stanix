---
title: devices and buses management
---
In stanix, every devices is described by a `device_t`, devices classes and devices can extend these devices.

## devices
A new device can be registered using `register_device`, and destroyed using `destroy_device`. When a device is destroyed it stay in memory until it's ref count drop to 0 (nobody use it). When a new device is registered a new device file is created in the devfs (most of the time mounted under `/dev`).

### device types
Three types of devices exist. The type is specified by the `type` field in the `device_t`.
- `DEVICE_CHAR`
  Character device.
- `DEVICE_BLOCK`
  Block device.
- `DEVICE_BUS`
  Bus that contain a list of `bus_addr_t`.
- `DEVICE_UNPLUGED`
  The device was destroyed.

### device number
Each device has a associed device number (`dev_t`). A `device_t` can be get from a device number using `device_from_number`. When `register_device` is called, the driver can provide the minor device number in the `number` field of the `device_t`, else the minor number is automaticly dynamicly allocated.

## buses
A bus (device class `bus_t` that extend `device_t`) is a special case of device that contain a list of `bus_addr_t`. Each `bus_addr_t` can then be controlled by another driver,in which case another `device_t` is linked to the `bus_addr_t`, which itself can also be a bus (e.g. a USB bus on a PCI bus).

### bus addresses
When a bus addresses is drived by a device driver, it is linked to a `device_t` using the `device` field of the `bus_addr_t` and so is the `device_t` linked to the `bus_addr_t` using the `addr` field. NOTE : When `register_device` is called, if the `addr` field point to an `bus_addr_t`, the device manager will automaticly link the bus address back to the device.

## drivers
A device driver (`device_driver_t`) can be registered using `register_device_driver` and unregistered using `unregister_device_driver`. A device driver can provide some function to discover new devices compatible with this driver.

## driver number
Each driver has a major dev number associed and all devices drived by this driver have the same major number.

### driver operations
- `check`
  Check if a specific `bus_addr_t` can be controlled by the device driver.
- `probe`
  Try to init a device on `bus_addr_t` using this driver, the `check` operation is always done before `probe`.

### driver priority
Sometimes multiples drivers can drive the same device (e.g. a BGA card can be drived by a standard VGA driver or a BGA driver). In this case, the priority field of all the concurrent device drivers are compared and the one with the highest priority win. If the device was already controlled by a lowest priority driver before (e.g. a new kernel module with a highest priority get loaded but the device is already controlled), the old device is destroyed using `destroy_device` and the device manager reinit the device using the highest priority driver.
