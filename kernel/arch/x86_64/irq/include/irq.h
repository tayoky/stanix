#ifndef IRQ_H
#define IRQ_H

#include <kernel/arch.h>
#include <stdint.h>

void init_irq(void);

void irq_map(void *handler,int irq_num);
void irq_mask(int irq_num);
void irq_eoi(int irq_num);
void irq_generic_map(void *handler, int irq_num, void (data);
void irq_handler(fault_frame *frame);

#define PIC_APIC 0x01
#define PIC_PIC  0x02

#endif
