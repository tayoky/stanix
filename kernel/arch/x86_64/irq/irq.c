#include "irq.h"
#include "idt.h"
#include "pic.h"
#include "kernel.h"
#include "print.h"
#include "panic.h"

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

void *handlers[16];

void init_irq(void){
	kstatus("init irq chip... ");

	//TODO : implement and use APIC if avalible
	init_pic();

	//set generic handler
	set_idt_gate(kernel->idt,32,irq0,0x8E);
	set_idt_gate(kernel->idt,33,irq1,0x8E);
	set_idt_gate(kernel->idt,34,irq2,0x8E);
	set_idt_gate(kernel->idt,35,irq3,0x8E);
	set_idt_gate(kernel->idt,36,irq4,0x8E);
	set_idt_gate(kernel->idt,37,irq5,0x8E);
	set_idt_gate(kernel->idt,38,irq6,0x8E);
	set_idt_gate(kernel->idt,39,irq7,0x8E);
	set_idt_gate(kernel->idt,40,irq8,0x8E);
	set_idt_gate(kernel->idt,41,irq9,0x8E);
	set_idt_gate(kernel->idt,42,irq10,0x8E);
	set_idt_gate(kernel->idt,43,irq11,0x8E);
	set_idt_gate(kernel->idt,44,irq12,0x8E);
	set_idt_gate(kernel->idt,45,irq13,0x8E);
	set_idt_gate(kernel->idt,46,irq14,0x8E);
	set_idt_gate(kernel->idt,47,irq15,0x8E);

	kok();

	if(kernel->pic_type == PIC_PIC){
		kinfof("use pic\n");
	} else if (kernel->pic_type == PIC_APIC){
		kinfof("use apic\n");
	}
}

void irq_map(void *handler,uint64_t irq_num){
	//first add it into the IDT
	set_idt_gate(kernel->idt,irq_num + 32,handler,0x8E);

	//and then unmask it
	switch (kernel->pic_type)
	{
	case PIC_PIC:
		pic_unmask(irq_num);
		break;
	
	default:
		//well hum .. we use an unknow irq chip 
		//should not be possible
		break;
	}
}

void irq_generic_map(void *handler,uint64_t irq_num){
	//save the handler
	handlers[irq_num] = handler;

	//and then unmask it
	switch (kernel->pic_type)
	{
	case PIC_PIC:
		pic_unmask(irq_num);
		break;
	
	default:
		//well hum .. we use an unknow irq chip 
		//should not be possible
		break;
	}
}

void irq_mask(uint64_t irq_num){
	switch (kernel->pic_type)
	{
	case PIC_PIC:
		pic_mask(irq_num);
		break;
	
	default:
		//well hum .. we use an unknow irq chip 
		//should not be possible
		break;
	}
}

void irq_eoi(uint64_t irq_num){
	switch (kernel->pic_type)
	{
	case PIC_PIC:
		pic_eoi(irq_num);
		break;
	
	default:
		//well hum .. we use an unknow irq chip 
		//should not be possible
		break;
	}
}

void irq_handler(fault_frame *frame){
	void (*handler)(fault_frame *) = handlers[frame->err_type];
	if(handler){
		handler(frame);
	}
	irq_eoi(frame->err_type);
}