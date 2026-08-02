#ifndef IDE_H
#define IDE_H

// ide specific ata shit

#include <kernel/bus.h>
#include <kernel/mutex.h>
#include <kernel/resource.h>

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
	uint8_t nIEN;
} ide_channel_t;

typedef struct ide_controller {
	ide_resource_t *base1;
	ide_resource_t *base2;
	ide_resource_t *ctrl1;
	ide_resource_t *ctrl2;
	ide_resource_t *bmide;
} ide_controller_t;

int ide_controller_pci_check(devnode_t *devnode);
int ide_controller_pci_probe(devnode_t *devnode);

#endif
