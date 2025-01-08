#ifndef KERNEL_H
#define KERNEL_H
#include <stdint.h>
#include "gdt.h"
#include "idt.h"
#include "limine.h"
#include "bootinfo.h"
#include "bitmap.h"

typedef struct kernel_table_struct{
	gdt_segment gdt[5];
	GDTR gdtr;
	idt_gate idt[256];
	IDTR idtr;
	bootinfo_table bootinfo;

	//in bytes
	uint64_t total_memory;
	//in bytes
	uint64_t used_memory;

	bitmap_meta bitmap;
	uint64_t hhdm;
	struct limine_memmap_response *memmap;
}kernel_table;

void halt();

#endif