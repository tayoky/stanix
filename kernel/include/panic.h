#ifndef PANIC_H
#define PANIC_H
#include <stdint.h>
#include <kernel/arch.h>

void panic(const char *error,fault_frame *fault);

#ifndef NULL
#define NULL (void *)0
#endif

#endif