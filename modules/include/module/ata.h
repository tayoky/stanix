#ifndef MODULE_ATA_H
#define MODULE_ATA_H

#include <kernel/bus.h>
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

struct ata_driver;

typedef struct {
	devnode_t devnode;
	devnode_t *channel;
	size_t sectors_count;
	uint32_t command_sets;
	uint32_t signature;
	char model[40];
} ata_device_t;

typedef struct ata_command {
	uint8_t opcode;
	uint8_t flags;
	uint64_t lba;
	size_t sectors_count;
	void *buf;
} ata_command_t;

#define ATA_CMD_SEND_LBA28     0x01
#define ATA_CMD_SEND_LBA48     0x02
#define ATA_CMD_READ_BUF       0x04
#define ATA_CMD_WRITE_BUF      0x08

typedef struct ata_driver {
	driver_t driver;
	int (*send_ata_command)(devnode_t *channel, devnode_t *devnode, ata_command_t *command);
	uint32_t (*get_signature)(devnode_t *channel, devnode_t *devnode);
} ata_driver_t;

static inline int ata_channel_send_command(devnode_t *channel, devnode_t *devnode, ata_command_t *command) {
	ata_driver_t *ata_driver = container_of(channel->driver, ata_driver_t, driver);
	if (ata_driver->send_ata_command) {
		return ata_driver->send_ata_command(channel, devnode, command);
	}
	return -ENOTSUP;
}

static inline uint32_t ata_channel_get_signature(devnode_t *channel, devnode_t *devnode) {
	ata_driver_t *ata_driver = container_of(channel->driver, ata_driver_t, driver);
	if (ata_driver->get_signature) {
		return ata_driver->get_signature(channel, devnode);
	}
	return 0xffffffff;
}

static inline int ata_send_command(devnode_t *devnode, ata_command_t *command) {
	return ata_channel_send_command(devnode->parent, devnode, command);
}

static inline uint32_t ata_get_signature(devnode_t *devnode) {
	return ata_channel_get_signature(devnode->parent, devnode);
}

#endif
