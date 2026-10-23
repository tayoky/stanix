#ifndef KERNEL_ASM_H
#define KERNEL_ASM_H

#if defined(__KERNEL__) || defined(__MODULE__)

#include <stdint.h>
#include <kernel/port.h>

static inline int arch_have_interrupt(void) {
    uintptr_t flags;
    asm volatile ("pushf\n"
                  "pop %0" : "=rm"(flags));

    return (flags & (1U << 9)) ;
}

static inline void arch_disable_interrupt(void) {
	asm volatile("cli");
}

static inline void arch_enable_interrupt(void) {
	asm volatile("sti");
}

static inline int arch_save_and_disable_interrupt(void) {
	int ret = arch_have_interrupt();
	arch_disable_interrupt();
	return ret;
}

static inline void arch_restore_interrupt(int saved) {
	if (saved) arch_enable_interrupt();
}

static inline void arch_halt(void) {
	arch_disable_interrupt();
	for (;;) ;
}

static inline void arch_pause(void) {
   asm volatile("hlt");
}

#define IA32_PAT_MSR 0x277
#define IA32_APIC_BASE 0x1B

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
