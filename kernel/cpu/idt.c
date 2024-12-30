#include "idt.h"
#include "kernel.h"
#include "print.h"
#include <stdint.h>
#include "interrupt.h"

void set_idt_gate(idt_gate *idt,uint8_t index,void *offset,uint8_t flags){
	idt[index].offset1 = (uint64_t)offset & 0xFFFF;
	idt[index].offset2 = ((uint64_t)offset >> 16) & 0xFFFF;
	idt[index].offset3 = ((uint64_t)offset >> 32) & 0xFFFFFFFF;

	//make sure the present bit is set
	idt[index].flags = flags | 0x80;
	idt[index].reserved = 0;
	idt[index].selector = 0;
}

void init_idt(kernel_table *kernel){
	kstatus("init IDT ...");
	set_idt_gate(kernel->idt,0,&divide_exception,0x8E);

	//create the IDTR
	kernel->idtr.size = sizeof(kernel->idt);
	kernel->idtr.offset = &kernel->idt;
	kok();
}