#ifndef KERNEL_IRQ_H
#define KERNEL_IRQ_H

#include <kernel/interrupt.h>
#include <kernel/arch.h>
#include <stdint.h>

void init_irq(void);
void irq_mask(irqnum_t irq_num);
void irq_unmask(irqnum_t irq_num);
void irq_eoi(irqnum_t irq_num);
void irq_register_handler(irqnum_t irq_num, interrupt_handler_t handler, void *data);
irqnum_t irq_allocate(interrupt_handler_t handler, void *data);
void irq_handler(registers_t *frame);

#endif
