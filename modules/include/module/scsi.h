#ifndef MODULE_SCSI_H
#define MODULE_SCSI_H

#include <kernel/bus.h>
#include <kernel/ioreq.h>
#include <stdint.h>

#define SCSI_OPCODE_6BYTE            (0b000 << 5)
#define SCSI_OPCODE_10BYTE           (0b001 << 5)
#define SCSI_OPCODE_10BYTE_SECONDARY (0b010 << 5)
#define SCSI_OPCODE_16BYTE           (0b100 << 5)
#define SCSI_OPCODE_12BYTE           (0b101 << 5)
#define SCSI_OPCODE_COMMAND          0x1f

typedef struct scsi_generic {
	uint8_t opcode;
	uint8_t data[15];
} scsi_generic_t;

typedef struct scsi_data16 {
	uint8_t high;
	uint8_t low;
} __attribute__((packed)) scsi_data16_t;

typedef struct scsi_inquiry {
	uint8_t opcode;
	uint8_t flags;
	uint8_t page_code;
	scsi_data16_t allocation_lenght;
	uint8_t control;
} __attribute__((packed)) scsi_inquiry_t;

#define SCSI_CMD_INQUIRY 0x12

#define SCSI_INQUIRY_EVPD 0x01

typedef struct scsi_inquiry_data {
	uint8_t peripheral;
	uint8_t rmb;
	uint8_t version;
	uint8_t data_format;
	uint8_t additional_lenght;
	uint8_t flags1;
	uint8_t flags2;
	uint8_t flags3;
	uint8_t vendor_ident[8];
	uint8_t product_ident[8];
	uint8_t product_revision[8];
	uint8_t serial[8];
	uint8_t vendor_unique[8];
	uint8_t reserved0;
	uint8_t reserved1;
	scsi_data16_t version_descriptors[8];
	uint8_t reserved1[22];
} __attribute__((packed)) scsi_inquiry_data_t;

#define SCSI_INQUIRY_PERIPHERAL_QUALIFIER              (0b111 << 5)
#define SCSI_INQUIRY_PERIPHERAL_QUALIFIER_SUPPORTED   (0b000 << 5)
#define SCSI_INQUIRY_PERIPHERAL_QUALIFIER_UNCONNECTED (0b001 << 5)
#define SCSI_INQUIRY_PERIPHERAL_QUALIFIER_RESERVED    (0b010 << 5)
#define SCSI_INQUIRY_PERIPHERAL_QUALIFIER_UNSUPPORTED (0b011 << 5)
#define SCSI_INQUIRY_PERIPHERAL_TYPE 0x1f
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SBC4    0x00
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SSC3    0x01
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SSC     0x02
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SPC2    0x03
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SBC_ALT 0x04
#define SCSI_INQUIRY_PERIPHERAL_TYPE_MMC5    0x05
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SBC     0x07
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SMC3    0x08
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SCC2    0x0c
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SES     0x0d
#define SCSI_INQUIRY_PERIPHERAL_TYPE_RBC     0x0e
#define SCSI_INQUIRY_PERIPHERAL_TYPE_OCRW    0x0f
#define SCSI_INQUIRY_PERIPHERAL_TYPE_BCC     0x10
#define SCSI_INQUIRY_PERIPHERAL_TYPE_OSD     0x11
#define SCSI_INQUIRY_PERIPHERAL_TYPE_ADC2    0x12
#define SCSI_INQUIRY_PERIPHERAL_TYPE_UNKNOWN 0x1f

#define SCSI_INQUIRY_RMB 0x80 // device is removable

#define SCSI_VERSION_NONE 0x00
#define SCSI_VERSION_SPC  0x03
#define SCSI_VERSION_SPC2 0x04
#define SCSI_VERSION_SPC3 0x05
#define SCSI_VERSION_SPC4 0x06
#define SCSI_VERSION_SPC5 0x07
// TODO : SPC6 ??

#define SCSO_VENDOR_IDENT_SEAGATE "SEAGATE "

typedef struct scsi_device {
	devnode_t devnode;
	devnode_t *bus;
	list_t pending_commands;
} scsi_device_t;

typedef struct scsi_command {
	ioreq_t ioreq;
	scsi_generic_t data;
	list_node_t node;
	scsi_device_t *device;
	void *buf;
	size_t buf_size;
	uint8_t flags;
} scsi_command_t;

#define SCSI_CMD_READ_BUF  0x01
#define SCSI_CMD_WRITE_BUF 0x02

typedef struct scsi_driver {
	driver_t driver;
	int (*submit_scsi_command)(devnode_t *channel, scsi_device_t *device, scsi_command_t *command);
} scsi_driver_t;

scsi_command_t *scsi_create_command(scsi_device_t *device, void *data, size_t size);
int scsi_submit_command_sync(scsi_command_t *command);
void scsi_print_command(scsi_command_t *command);

scsi_device_t *scsi_create_device(devnode_t *bus);

#endif
