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
	idt[index].selector = 0x08;
}

const char *error_msg[] = {
	"divide by zero",
	"debug",
	"breakpoint",
	"overflow",
	"bound range exceeded",
	"invalid OPcode",
	"device not avalibe"
};

void exception_handler(){
	//get the error code from register rax so we can use it to know the error
	uint64_t error = 5;
	asm("mov %%ebx ,%%eax" : "=b" (error): );
	kprintf("error : code %u\n",error);
	if(error < (sizeof(error_msg) / sizeof(char *)))
	kprintf("%s\n",error_msg[error]);
	while(1);
	return;
}

void init_idt(kernel_table *kernel){
	kstatus("init IDT ...");
	set_idt_gate(kernel->idt,0,&divide_exception,0x8E);
	set_idt_gate(kernel->idt,4,&overflow_exception,0x8E);
	set_idt_gate(kernel->idt,14,&pagefault_exception,0x8E);

	//create the IDTR
	kernel->idtr.size = sizeof(kernel->idt);
	kernel->idtr.offset = &kernel->idt;
	//and load it
	asm("lidt %0" : : "m" (kernel->idtr));
	kok();
}