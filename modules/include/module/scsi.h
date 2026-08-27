#ifndef MODULE_SCSI_H
#define MODULE_SCSI_H

#include <kernel/bus.h>
#include <kernel/ioreq.h>
#include <sys/device.h>
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

#define SCSI_DEFINE_BITS(x) \
typedef struct scsi_data ## x { \
	uint8_t data[x / 8]; \
} __attribute__((packed)) scsi_data ## x ## _t; \
\
static inline scsi_data ## x ## _t scsi_uint ## x ## _to_data ## x(uint ## x ## _t data) { \
	scsi_data ## x ## _t scsi_data = {0};\
	for (int i = 0; i < sizeof(scsi_data.data); i++) {\
		scsi_data.data[i] = (uint8_t)(data >> (sizeof(scsi_data.data) - 1 - i) * 8);\
	}\
	return scsi_data;\
} \
\
static inline uint ## x ## _t scsi_data ## x ## _to_uint ## x(scsi_data ## x ## _t *scsi_data) { \
	uint ## x ## _t data = 0; \
	for (int i = 0; i < sizeof(scsi_data->data); i++) {\
		data |= (uint ## x ##_t)scsi_data->data[i] << ((sizeof(scsi_data->data) - 1 - i) * 8);\
	}\
	return data; \
}

SCSI_DEFINE_BITS(16)
SCSI_DEFINE_BITS(32)
SCSI_DEFINE_BITS(64)

typedef struct scsi_inquiry {
	uint8_t opcode;
	uint8_t flags;
	uint8_t page_code;
	scsi_data16_t allocation_length;
	uint8_t control;
} __attribute__((packed)) scsi_inquiry_t;

#define SCSI_INQUIRY_OPCODE 0x12
#define SCSI_INQUIRY_EVPD 0x01

typedef struct scsi_inquiry_data {
	uint8_t peripheral;
	uint8_t rmb;
	uint8_t version;
	uint8_t data_format;
	uint8_t additional_length;
	uint8_t flags1;
	uint8_t flags2;
	uint8_t flags3;
	char vendor[8];
	char product[16];
	char product_revision[4];
	char serial[8];
	uint8_t vendor_unique[8];
	uint8_t reserved0;
	uint8_t reserved1;
	scsi_data16_t version_descriptors[8];
	uint8_t reserved2[22];
} __attribute__((packed)) scsi_inquiry_data_t;

#define SCSI_INQUIRY_PERIPHERAL_QUALIFIER              (0b111 << 5)
#define SCSI_INQUIRY_PERIPHERAL_QUALIFIER_SUPPORTED   (0b000 << 5)
#define SCSI_INQUIRY_PERIPHERAL_QUALIFIER_UNCONNECTED (0b001 << 5)
#define SCSI_INQUIRY_PERIPHERAL_QUALIFIER_RESERVED    (0b010 << 5)
#define SCSI_INQUIRY_PERIPHERAL_QUALIFIER_UNSUPPORTED (0b011 << 5)
#define SCSI_INQUIRY_PERIPHERAL_TYPE 0x1f
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SBC4    0x00 // direct access block device
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SSC3    0x01 // sequential access device
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SSC     0x02 // printer device
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SPC2    0x03 // processor device
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SBC_ALT 0x04 // write-once device
#define SCSI_INQUIRY_PERIPHERAL_TYPE_MMC5    0x05 // CD/DVD device
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SBC     0x07 // optical memory device
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SMC3    0x08 // medium changer device
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SCC2    0x0c // storage array controller
#define SCSI_INQUIRY_PERIPHERAL_TYPE_SES     0x0d // enclosure services device
#define SCSI_INQUIRY_PERIPHERAL_TYPE_RBC     0x0e // simplified direct access device
#define SCSI_INQUIRY_PERIPHERAL_TYPE_OCRW    0x0f // optical card reader/writer device
#define SCSI_INQUIRY_PERIPHERAL_TYPE_BCC     0x10 // bridge controller device
#define SCSI_INQUIRY_PERIPHERAL_TYPE_OSD     0x11 // object based storage device
#define SCSI_INQUIRY_PERIPHERAL_TYPE_ADC2    0x12 // automation/driver interface
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

// READ CAPACITY(10) command
typedef struct scsi_read_capacity10 {
	uint8_t opcode;
	uint8_t reserved0;
	scsi_data32_t obselete;
	uint8_t reserved1[2];
	uint8_t reserved2;
	uint8_t control;
} __attribute__((packed)) scsi_read_capacity10_t;

#define SCSI_READ_CAPACITY10_OPCODE 0x25

typedef struct scsi_read_capacity10_data {
	scsi_data32_t max_lba;
	scsi_data32_t block_length;
} __attribute__((packed)) scsi_read_capacity10_data_t;

// READ(10) command
typedef struct scsi_read10 {
	uint8_t opcode;
	uint8_t flags;
	scsi_data32_t lba;
	uint8_t group_number;
	scsi_data16_t transfer_length;
	uint8_t control;
} __attribute__((packed)) scsi_read10_t;

#define SCSI_READ10_OPCODE 0x28
#define SCSI_READ_RDPROTECT 0xe0
#define SCSI_READ_DPO       0x10 // disable page out
#define SCSI_READ_FUA       0x08 // force unit access (bypass cache)
#define SCSI_READ_RARC      0x04

// READ TOC command
typedef struct scsi_read_toc {
	uint8_t opcode;
	uint8_t msf;
	uint8_t format;
	uint8_t reserved0;
	uint8_t reserved1;
	uint8_t reserved2;
	union __attribute__((packed)) {
		uint8_t track_number;
		uint8_t session_number;
	};
	scsi_data16_t allocation_length;
	uint8_t control;
} __attribute((packed)) scsi_read_toc_t;

#define SCSI_READ_TOC_OPCODE 0x43
#define SCSI_READ_TOC_FORMAT 0x0f
#define SCSI_READ_TOC_FORMAT_FORMATED_TOC   0x00
#define SCSI_READ_TOC_FORMAT_MULTI_SESSIONS 0x01
#define SCSI_READ_TOC_FORMAT_RAW_TOC        0x02

#define SCSI_READ_TOC_MAX_TRACKS 100

typedef struct scsi_read_toc_data {
	scsi_data16_t data_length;
	union __attribute__((packed)) {
		uint8_t first_track;
		uint8_t first_session;
	};
	union __attribute__((packed)) {
		uint8_t last_track;
		uint8_t last_session;
	};
	union {
		struct {
			uint8_t reserved0;
			uint8_t flags;
			uint8_t track_number;
			uint8_t reserved1;
			scsi_data32_t track_start;
		} __attribute__((packed)) formatted_toc[SCSI_READ_TOC_MAX_TRACKS];
		struct {
			uint8_t reserved0;
			uint8_t flags;
			uint8_t first_track;
			uint8_t reserved1;
			scsi_data32_t first_track_start;
		} __attribute__((packed)) multi_sessions[SCSI_READ_TOC_MAX_TRACKS];
	};
} __attribute__((packed)) scsi_read_toc_data_t;

#define SCSI_READ_TOC_LEAD_OUT 0xaa
#define SCSI_READ_TOC_CONTROL 0x0f
#define SCSI_READ_TOC_CONTROL_PRE_EMPHASIS   0x01
#define SCSI_READ_TOC_CONTROL_COPY_PERMITTED 0x02
#define SCSI_READ_TOC_CONTROL_DATA           0x04
#define SCSI_READ_TOC_CONTROL_4CHANNEL       0x08
#define SCSI_READ_TOC_ADR      0xf0

// READ(12) command
typedef struct scsi_read12 {
	uint8_t opcode;
	uint8_t flags;
	scsi_data32_t lba;
	scsi_data32_t transfer_length;
	uint8_t group_number;
	uint8_t control;
} __attribute__((packed)) scsi_read12_t;

#define SCSI_READ12_OPCODE 0xa8

// READ CAPACITY(16) command
typedef struct scsi_read_capacity16 {
	uint8_t opcode;
	uint8_t service_action;
	scsi_data32_t obselete;
	scsi_data32_t allocation_length;
	uint8_t reserved;
	uint8_t control;
} __attribute__((packed)) scsi_read_capacity16_t;

#define SCSI_READ_CAPACITY16_OPCODE 0x9e

typedef struct scsi_read_capacity16_data {
	scsi_data64_t max_lba;
	scsi_data32_t block_length;
	uint8_t flags0;
	uint8_t flags1;
	uint8_t flags2;
	uint8_t flags3;
	uint8_t reserved[16];
} __attribute__((packed)) scsi_read_capacity16_data_t;

typedef struct scsi_device {
	devnode_t devnode;
	device_info_t info;
	devnode_t *bus;
	int type;
} scsi_device_t;

typedef struct scsi_command {
	ioreq_t ioreq;
	scsi_generic_t data;
	size_t data_length;
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
scsi_command_t *scsi_create_read_command(scsi_device_t *device, size_t lba, size_t transfer_length, uint8_t flags, void *buf, size_t buf_size);
void scsi_print_command(scsi_command_t *command);

scsi_device_t *scsi_create_device(devnode_t *bus);

#endif
