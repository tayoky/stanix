#ifndef _KERNEL_IRQ_H
#define _KERNEL_IRQ_H

#include <kernel/interrupt.h>
#include <kernel/arch.h>
#include <stdint.h>

void init_irq(void);
void irq_register_handler(int irq_num, interrupt_handler_t handler, void *data);
int irq_allocate(interrupt_handler_t handler, void *data);
void irq_eoi(int irq_num);
void irq_handler(fault_frame *frame);

#endif
