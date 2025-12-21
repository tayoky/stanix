#ifndef KERNEL_H
#define KERNEL_H
#include <stdint.h>
#include <sys/time.h>
#include <kernel/limine.h>
#include <kernel/bootinfo.h>
#include <kernel/pmm.h>
#include <kernel/kheap.h>
#include <kernel/vfs.h>
#include <kernel/terminal_emu.h>
#include <kernel/scheduler.h>
#include <kernel/arch.h>
#include <kernel/spinlock.h>

typedef struct kernel_table_struct{
	arch_specific arch;
	bootinfo_table bootinfo;
	struct limine_kernel_address_response *kernel_address;

	//in bytes
	uint64_t total_memory;
	//in bytes
	uint64_t used_memory;

	PMM_entry *PMM_stack_head;
	uint64_t hhdm;
	struct limine_memmap_response *memmap;
	uint64_t stack_start;
	heap_info kheap;
	vfs_mount_point *first_mount_point;
	struct limine_file *initrd;
	const char *conf_file;
	vfs_fd_t **outs;
	uint8_t pic_type;
	pid_t tid_count;
	task_t *current_task;
	char can_task_switch;
	spinlock PMM_lock;
	int pty_count;
}kernel_table;

extern kernel_table *kernel;
#endif