#include <kernel/scheduler.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/signal.h>
#include <kernel/interrupt.h>
#include <kernel/irq.h>
#include <stdint.h>
#include "idt.h"
#include "isr.h"
#include "panic.h"
#include "sys.h"

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

void page_fault_info(fault_frame *fault){
	kprintf("page fault at address 0x%lx\n",fault->cr2);
	if(fault->err_code & 0x04)kprintf("user");
	else kprintf("OS");
	kprintf(" has trying to ");
	if(fault->err_code & 0x10) kprintf("execute");
	else if(fault->err_code & 0x02)kprintf("write");
	else kprintf("read");
	kprintf(" a ");
	if(!(fault->err_code & 0x01))kprintf("non ");
	kprintf("present page\n");
}

void isr_handler(fault_frame *fault){
	if(fault->err_type >= 32 && fault->err_type <= 47){
		return irq_handler(fault);
	}
	//0x80 is syscall not a fault
	if(fault->err_type == 0x80){
		return syscall_handler(fault);
	}
	if(is_userspace(fault)){
		if(fault->err_type == 14 && fault->cr2 != MAGIC_SIGRETURN){
			kprintf("%p : segmentation fault (core dumped)\n",fault->rip);
			page_fault_info(fault);
		}
		fault_handler(fault);
		return;
	}
	kprintf("error : 0x%lx\n",fault->err_type);
	if(fault->err_type < (sizeof(error_msg) / sizeof(char *))){
		//show info for page fault
		if(fault->err_type == 14){
			page_fault_info(fault);
		}
		panic(error_msg[fault->err_type],fault);
	}else{
		panic("unkown fault",fault);
	}

	return;
}

void init_idt(void){
	kstatus("init IDT... ");

	//some exception other exceptions are not very important
	set_idt_gate(kernel->arch.idt,0,&divide_exception,0x8E);
	set_idt_gate(kernel->arch.idt,4,&overflow_exception,0x8E);
	set_idt_gate(kernel->arch.idt,6,&invalid_op_exception,0x8E);
	set_idt_gate(kernel->arch.idt,10,&invalid_tss_exception,0x8E);
	set_idt_gate(kernel->arch.idt,13,&global_fault_exception,0x8E);
	set_idt_gate(kernel->arch.idt,14,&pagefault_exception,0x8E);

	//syscall
	set_idt_gate(kernel->arch.idt,0x80,&isr128,0xEF);

	//create the IDTR
	kernel->arch.idtr.size = sizeof(kernel->arch.idt) - 1;
	kernel->arch.idtr.offset =(uint64_t) &kernel->arch.idt;
	//and load it
	asm("lidt %0" : : "m" (kernel->arch.idtr));
	kok();
}