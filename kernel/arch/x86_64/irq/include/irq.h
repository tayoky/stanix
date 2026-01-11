#ifndef IRQ_H
#define IRQ_H

#include <kernel/interrupt.h>
#include <kernel/arch.h>
#include <stdint.h>

void init_irq(void);
void irq_register_handler(int irq_num, interrupt_handler_t handler, void *data);
int irq_allocate(interrupt_handler_t handler, void *data);
void irq_eoi(int irq_num);
void irq_handler(fault_frame *frame);

#define PIC_APIC 0x01
#define PIC_PIC  0x02

#endif
