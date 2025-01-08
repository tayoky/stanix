#include "idt.h"
#include "kernel.h"
#include "print.h"
#include <stdint.h>
#include "interrupt.h"
#include "panic.h"

void set_idt_gate(idt_gate *idt,uint8_t index,void *offset,uint8_t flags){
	idt[index].offset1 = (uint64_t)offset & 0xFFFF;
	idt[index].offset2 = ((uint64_t)offset >> 16) & 0xFFFF;
	idt[index].offset3 = ((uint64_t)offset >> 32) & 0xFFFFFFFF;

	//make sure the present bit is set
	idt[index].flags = flags | 0x80;
	idt[index].reserved = 0;
	idt[index].selector = 0x08;
}

const char *error_msg[] = {
	"divide by zero",
	"debug",
	"non maskable",
	"breakpoint",
	"overflow",
	"bound range exceeded",
	"invalid OPcode",
	"device not avalibe",
	"double fault",
	"Coprocessor segment overrun \n ask tayoky if you see this",
	"invalid tss",
	"segment not present",
	"stack segment fault",
	"general protection fault",
	"page fault",
	"not an error",
	"x87 floating point fault",
	"alginement check",
	"machine check",
	"SIMD floating point fault",
	"virtualization exception",
	"control protection exception",
};

void exception_handler(uint64_t error){
	kprintf("error : code 0x%lx\n",error);
	regs registers;
	registers.cr2 = 0;
	if(error < (sizeof(error_msg) / sizeof(char *)))
		panic(error_msg[error],registers);
	else
	panic("",registers);

	return;
}

void init_idt(kernel_table *kernel){
	kstatus("init IDT ...");

	//some exception other exceptions are not very important
	set_idt_gate(kernel->idt,0,&divide_exception,0x8E);
	set_idt_gate(kernel->idt,4,&overflow_exception,0x8E);
	set_idt_gate(kernel->idt,6,&invalid_op_exception,0x8E);
	set_idt_gate(kernel->idt,10,&invalid_tss_exception,0x8E);
	set_idt_gate(kernel->idt,13,&global_fault_exception,0x8E);
	set_idt_gate(kernel->idt,14,&pagefault_exception,0x8E);

	//create the IDTR
	kernel->idtr.size = sizeof(kernel->idt);
	kernel->idtr.offset =(uint64_t) &kernel->idt;
	//and load it
	asm("lidt %0" : : "m" (kernel->idtr));
	kok();
}