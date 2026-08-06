#ifndef IDE_H
#define IDE_H

// ide specific ata shit

#include <kernel/bus.h>
#include <kernel/mutex.h>
#include <kernel/resource.h>
#include <module/ata.h>

#define IDE_RID_BASE  1
#define IDE_RID_CTRL  2
#define IDE_RID_BMIDE 3

typedef struct ide_device {
	devnode_t devnode;
	// TODO : add various fields
} ide_device_t;

typedef struct ide_channel {
	mutex_t mutex;
	resource_t *base;
	resource_t *ctrl;
	resource_t *bmide;
	devnode_t *master;
	devnode_t *slave;
	uint32_t master_signature;
	uint32_t slave_signature;
	uint8_t nIEN;
} ide_channel_t;

typedef struct ide_controller {
	resource_t *base1;
	resource_t *base2;
	resource_t *ctrl1;
	resource_t *ctrl2;
	resource_t *bmide;
} ide_controller_t;

extern driver_t ide_controller_driver;
extern ata_driver_t ide_channel_driver;

#endif
