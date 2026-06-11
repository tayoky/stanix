#ifndef KERNEL_H
#define KERNEL_H
#include <stdint.h>
#include <sys/time.h>
#include <kernel/limine.h>
#include <kernel/bootinfo.h>
#include <kernel/vfs.h>
#include <kernel/arch.h>

typedef struct kernel_table_struct{
	arch_specific arch;
	bootinfo_table bootinfo;
	struct limine_kernel_address_response *kernel_address;
	uint64_t hhdm;
	struct limine_memmap_response *memmap;
	struct limine_file *initrd;
	const char *conf_file;
	vfs_fd_t **outs;
}kernel_table;

extern kernel_table *kernel;
#endif