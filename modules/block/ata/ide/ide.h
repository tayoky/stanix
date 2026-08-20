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
#define IDE_RID_IRQ   4

typedef struct ide_channel {
	mutex_t mutex;
	resource_t *base;
	resource_t *ctrl;
	resource_t *bmide;
	resource_t *irq;
	ata_device_t *master;
	ata_device_t *slave;
	uint8_t nIEN;
} ide_channel_t;

typedef struct ide_channel_resources {
	resource_t *base;
	resource_t *ctrl;
	resource_t *irq;
} ide_channel_resources_t;

typedef struct ide_controller {
	ide_channel_resources_t channel_res[2];
	resource_t *bmide;
	resource_t *shared_irq;
} ide_controller_t;

extern driver_t ide_controller_driver;
extern ata_driver_t ide_channel_driver;
extern int disable_irq = 0;

#endif
