#ifndef DEV_H
#define DEV_H
#include <stdint.h>
#include <stddef.h>
#include "vfs.h"
#include "tmpfs.h"


typedef struct {
	device_op *device;
	void *private_inode;
}device_inode;

void init_devices(void);

int create_dev(const char *path,device_op *device,void *private_inode);

#endif