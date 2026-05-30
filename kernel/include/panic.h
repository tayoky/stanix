#ifndef KERNEL_PANIC_H
#define KERNEL_PANIC_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/arch.h>

void panic(const char *error, registers_t *fault);

#endif
