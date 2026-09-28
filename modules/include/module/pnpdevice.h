#ifndef MODULE_PNPDEVICE_H
#define MODULE_PNPDEVICE_H

#include <kernel/bus.h>

typedef struct pnp_device {
	devnode_t devnode;
	char *pnp_id;
} pnp_device_t;

#define PNP_RID_IOPORT(x) 100000 + x
#define PNP_RID_IRQ(x)    200000 + x
#define PNP_RID_MEMORY(x) 300000 + x

#endif
