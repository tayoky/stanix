#ifndef KERNEL_ASM_H
#define KERNEL_ASM_H

#if defined(__KERNEL__) || defined(MODULE)

#include <stdint.h>
#include <kernel/port.h>

#define disable_interrupt() asm("cli")
#define enable_interrupt() asm("sti")
#define halt() while(1)

static inline int have_interrupt(){
    uintptr_t flags;
    asm volatile ("pushf\n"
                  "pop %0" : "=rm"(flags));

    return (flags & (1 << 9)) ;
}

static inline arch_pause(void) {
   asm volatile("hlt");
}

#define IA32_PAT_MSR 0x277

static inline void rdmsr(uint32_t msr, uint32_t *low, uint32_t *high) {
   asm volatile("rdmsr" : "=a"(*low), "=d"(*high) : "c"(msr));
}

static inline void wrmsr(uint32_t msr, uint32_t low, uint32_t high) {
   asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

static inline void io_wait(void) {
   out_byte(0x80, 0);
}

#endif

#endif