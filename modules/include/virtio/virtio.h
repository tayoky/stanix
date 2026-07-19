#ifndef _VIRTIO_H
#define _VIRTIO_H

#define VIRTIO_STATUS_ACKNOWLEDGE       1
#define VIRTIO_STATUS_DRIVER            2   // driver found for this device
#define VIRTIO_STATUS_FAILED            128 // fatal error happend
#define VIRTIO_STATUS_FEATURES_OK       8   // features negotiation is finished
#define VIRTIO_STATUS_DRIVER_OK         4
#define VIRTIO_STATUS_DEVICE_NEED_RESET 64

typedef struct virtio_queue_descriptor {
	uint64_t addr;
	uint32_t length;
	uint16_t flags;
	uint16_t next;
} __attribute__((packed)) virtio_queue_descriptor_t;

#define VIRTIO_DESC_F_NEXT      1 // continuing via next field
#define VIRTIO_DESC_F_WRITE     2 // write only for device (else read only for device)
#define VIRTIO_DESC_F_INDIRECT  4 // buffer contain a list of buffer descriptor

#endif
