#include "irq.h"
#include "idt.h"
#include "pic.h"
#include "kernel.h"
#include "print.h"

void init_irq(void){
	kstatus("init irq chip... ");

	//TODO : implement and use APIC if avalible
	init_pic();

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