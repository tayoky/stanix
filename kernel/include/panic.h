#ifndef KERNEL_PANIC_H
#define KERNEL_PANIC_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/arch.h>
#include <kernel/attributes.h>

ATTR_NORETURN void panic(const char *error, registers_t *fault);

#endif
