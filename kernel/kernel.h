#ifndef KERNEL_H
#define KERNEL_H
#include "gdt.h"
#include "idt.h"

typedef struct kernel_table_struct{
	gdt_segment gdt[5];
	GDTR gdtr;
	idt_gate idt[256];
	IDTR idtr;
}kernel_table;

#endif