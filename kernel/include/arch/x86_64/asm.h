#ifndef _KERNEL_ASM_H
#define _KERNEL_ASM_H

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

#define IA32_PAT_MSR 0x277

static inline void rdmsr(uint32_t msr, uint32_t *lower, uint32_t *higger) {
   asm volatile("rdmsr" : "=a"(*lower), "=d"(*higger) : "c"(msr));
}

static inline void wrmsr(uint32_t msr, uint32_t lower, uint32_t higger) {
   asm volatile("wrmsr" : : "a"(lower), "d"(higger), "c"(msr));
}

#endif