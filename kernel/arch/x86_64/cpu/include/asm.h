#ifndef ASM_H
#define ASM_H

#include <stdint.h>

#define disable_interrupt() asm("cli")
#define enable_interrupt() asm("sti")
#define halt() while(1)

#if defined(__KERNEL__) || defined(MODULE)
static inline int have_interrupt(){
    uintptr_t flags;
    asm volatile ("pushf\n"
                  "pop %0" : "=rm"(flags));

    return (flags & (1 << 9)) ;
}
#endif

#endif