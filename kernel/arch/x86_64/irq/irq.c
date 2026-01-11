#include "irq.h"
#include "idt.h"
#include "pic.h"
#include <kernel/interrupt.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
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

static interrupt_handler_t handlers[256 - 32];
static void *handlers_data[256 - 32];

void init_irq(void){
	kstatusf("init irq chip... ");

	//TODO : implement and use APIC if avalible
	init_pic();
 
	kok();

	if(kernel->pic_type == PIC_PIC){
		kinfof("use pic\n");
	} else if (kernel->pic_type == PIC_APIC){
		kinfof("use apic\n");
	}
}


int irq_allocate(interrupt_handler_t handler, void *data) {
	int i = 64;
	while (i<256 - 32) {
		if (!handlers[i]) {
			handlers[i] = handler;
			irq_register_handler(i, handler, data);
			return i;
		}
		i++;
	}
	return -1;
}

void irq_register_handler(int irq_num, interrupt_handler_t handler, void *data) {
	// save the handler
	handlers[irq_num] = handler;
	handlers_data[irq_num] = data;

	// and then mask/unmask it
	switch (kernel->pic_type)
	{
	case PIC_PIC:
		if (handler) {
			pic_unmask(irq_num);
		} else {
			pic_mask(irq_num);
		}
		break;
	
	default:
		//well hum .. we use an unknow irq chip 
		//should not be possible
		break;
	}
}

void irq_eoi(int irq_num){
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

void irq_handler(fault_frame *frame) {
	interrupt_handler_t handler = handlers[frame->err_type - 32];
	void *data = handlers_data[frame->err_type - 32];
	frame->err_code = frame->err_type - 32;
	if(handler){
		handler(frame, data);
	}
	// if the err_code is -1 then it aready send eoi
	if(frame->err_code != (uintptr_t)-1 && frame->err_type < 48){
		irq_eoi(frame->err_code);
	}
}
