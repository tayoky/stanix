#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <kernel/arch.h>

typedef void (*interrupt_handler_t)(fault_frame_t *frame, void *data);

void timer_handler(fault_frame_t *frame);
int fault_handler(fault_frame_t *frame);
int irq_allocate(interrupt_handler_t handler, void *data);

#endif
