#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

void init_irq(void);

void irq_map(void *handler,uintmax_t irq_num);
void irq_mask(uintmax_t irq_num);
void irq_eoi(uintmax_t irq_num);
void irq_generic_map(void *handler,uintmax_t irq_num);

#define PIC_APIC 0x01
#define PIC_PIC  0x02

#endif