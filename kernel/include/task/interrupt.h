#ifndef KERNEL_INTERRUPT_H
#define KERNEL_INTERRUPT_H

#include <kernel/arch.h>

typedef void (*interrupt_handler_t)(registers_t *frame, void *data);

void timer_handler(registers_t *frame);
int page_fault_handler(registers_t *frame);
int fpu_fault_handler(registers_t *frame);

// TODO : see if we implement these
//void *interrupt_register_handler(intrnum_t num, interrupt_handler_t handler, void *data);
//void interrupt_unregister_handler(intrnum_t num, void *handle);

#endif
