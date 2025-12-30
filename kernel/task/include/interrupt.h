#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <kernel/arch.h>

typedef void (*interrupt_handler_t)(fault_frame *frame, void *data);

void timer_handler(fault_frame *frame);
void fault_handler(fault_frame *frame);

#endif
