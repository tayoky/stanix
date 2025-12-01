#ifndef DEV_H
#define DEV_H
#include <stdint.h>
#include <stddef.h>

#include <kernel/vfs.h>


void init_devices(void);
int devfs_create_dev(const char *path, vfs_node *dev) ;

#endif