#ifndef FD_H
#define FD_H

#include "vfs.h"
#include <stdint.h>

typedef struct {
	vfs_node *node;
	uint64_t offset;
	uint64_t present;
}file_descriptor;

int is_vaild_fd(int fd);

#endif