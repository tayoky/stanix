#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <kernel/arch.h>

void timer_handler(fault_frame *frame);
void fault_handler(fault_frame *frame);

#endif