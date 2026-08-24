#ifndef MODULE_ATA_H
#define MODULE_ATA_H

#include <kernel/bus.h>
#include <kernel/ioreq.h>
#include <kernel/list.h>
#include <stdint.h>
#include <errno.h>

// thanks osdev wiki for this constants

// ata commands
#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_READ_PIO_EXT    0x24
#define ATA_CMD_READ_DMA        0xC8
#define ATA_CMD_READ_DMA_EXT    0x25
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_WRITE_PIO_EXT   0x34
#define ATA_CMD_WRITE_DMA       0xCA
#define ATA_CMD_WRITE_DMA_EXT   0x35
#define ATA_CMD_CACHE_FLUSH     0xE7
#define ATA_CMD_CACHE_FLUSH_EXT 0xEA
#define ATA_CMD_PACKET          0xA0
#define ATA_CMD_IDENTIFY_PACKET 0xA1
#define ATA_CMD_IDENTIFY        0xEC

#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

// thanks Sasdallas for this
typedef struct ata_ident {
	uint16_t flags;           // If bit 15 is cleared, valid drive. If bit 7 is set to one, this is removable.
	uint16_t obsolete;        // Obsolete
	uint16_t specifics;       // 7.17.7.3 in specification
	uint16_t obsolete2[6];    // Obsolete
	uint16_t obsolete3;       // Obsolete
	char serial[20];          // Serial number
	uint16_t obsolete4[3];    // Obsolete
	char firmware[8];         // Firmware revision
	char model[40];           // Model number
	uint16_t rw_multiple;     // R/W multiple support (<=16 is SATA)
	uint16_t obsolete5;       // Obsolete
	uint32_t capabilities;    // Capabilities of the IDE device
	uint16_t obsolete6[2];    // Obsolete
	uint16_t field_validity;  // If 1, the values reported in _ - _ are valid
	uint16_t obsolete7[5];    // Obsolete
	uint16_t multi_sector;    // Multiple sector setting
	uint32_t sectors;         // Total addressable sectors
	uint16_t obsolete8[20];   // Technically these aren't obsolete, but they contain nothing really useful
	uint32_t command_sets;    // Command/feature sets
	uint16_t obsolete9[16];   // Contain nothing really useful
	uint64_t sectors_lba48;   // LBA48 maximum sectors, AND by 0000FFFFFFFFFFFF for validity
	uint16_t obsolete10[152]; // Contain nothing really useful
} __attribute__((packed)) __attribute__((aligned(16))) ata_ident_t;

typedef struct ata_common_ident {
	uint32_t command_sets;
	char model[41];
	char firmware[9];
	char serial[21];
} ata_common_ident_t;

struct ata_driver;

typedef struct {
	devnode_t devnode;
	list_t pending_commands;
	devnode_t *channel;
	uint32_t signature;
} ata_device_t;

typedef struct ata_regs {
	uint8_t command;
	uint8_t device;
	uint8_t lba0;
	uint8_t lba1;
	uint8_t lba2;
	uint8_t lba3;
	uint8_t lba4;
	uint8_t lba5;
	uint16_t sectors_size;
} ata_regs_t;

typedef struct ata_command {
	ioreq_t ioreq;
	list_node_t node;
	ata_regs_t regs;
	uint64_t lba;
	uint16_t sectors_count;
	void *buf;
	ata_device_t *device;
	uint8_t opcode;
	uint8_t flags;
	uint8_t scsi_cmd[16];
} ata_command_t;

#define ATA_CMD_SEND_LBA28     0x01
#define ATA_CMD_SEND_LBA48     0x02
#define ATA_CMD_READ_BUF       0x04
#define ATA_CMD_WRITE_BUF      0x08
#define ATA_CMD_SCSI           0x10

typedef struct ata_driver {
	driver_t driver;
	int (*submit_ata_command)(devnode_t *channel, ata_device_t *device, ata_command_t *command);
} ata_driver_t;

#define ATA_BUSES BUSES("ide_channel")

ata_command_t *ata_create_command(ata_device_t *device);
int ata_submit_command_sync(ata_command_t *command);

void ata_parse_common_ident(ata_common_ident_t *common_ident, ata_ident_t *ident);
#endif
