#ifndef KERNEL_H
#define KERNEL_H
#include <stdint.h>
#include <sys/time.h>
#include <kernel/limine.h>
#include <kernel/vfs.h>

typedef struct kernel_table_struct{
	const char *conf_file;
	vfs_fd_t **outs;
}kernel_table;

extern kernel_table *kernel;
#endif