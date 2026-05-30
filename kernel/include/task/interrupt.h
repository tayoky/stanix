#ifndef KERNEL_INTERRUPT_H
#define KERNEL_INTERRUPT_H

#include <kernel/arch.h>

typedef void (*interrupt_handler_t)(registers_t *frame, void *data);

void timer_handler(registers_t *frame);
int fault_handler(registers_t *frame);
int irq_allocate(interrupt_handler_t handler, void *data);

#endif
