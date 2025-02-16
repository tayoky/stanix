#ifndef KERNEL_H
#define KERNEL_H
#include <stdint.h>
#include <sys/time.h>
#include "gdt.h"
#include "idt.h"
#include "limine.h"
#include "bootinfo.h"
#include "bitmap.h"
#include "kheap.h"
#include "vfs.h"
#include "terminal_emu.h"
#include "scheduler.h"
#include "tss.h"

extern struct timeval time;

typedef struct kernel_table_struct{
	gdt_segment gdt[7];
	GDTR gdtr;
	idt_gate idt[256];
	IDTR idtr;
	TSS tss;
	bootinfo_table bootinfo;
	struct limine_kernel_address_response *kernel_address;

	//in bytes
	uint64_t total_memory;
	//in bytes
	uint64_t used_memory;

	bitmap_meta bitmap;
	uint64_t hhdm;
	struct limine_memmap_response *memmap;
	uint64_t stack_start;
	kheap_info kheap;
	vfs_mount_point *first_mount_point;
	struct limine_file *initrd;
	const char *conf_file;
	vfs_node **outs;
	terminal_emu_settings terminal_settings;
	uint8_t pic_type;
	pid_t created_proc_count;
	process *current_proc;
	char can_task_switch;
}kernel_table;

extern kernel_table *kernel;
#endif