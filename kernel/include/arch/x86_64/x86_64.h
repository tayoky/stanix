#ifndef KERNEL_X86_64_H
#define KERNEL_X86_64_H

#include <kernel/apic.h>
#include <kernel/asm.h>
#include <kernel/cmos.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/isr.h>
#include <kernel/paging.h>
#include <kernel/pic.h>
#include <kernel/pit.h>
#include <kernel/port.h>
#include <kernel/serial.h>
#include <kernel/string.h>
#include <kernel/tss.h>
#include <sys/shutdown.h>
#include <stdint.h>

// any change here must be replicated in interrupt handler
// and context switch
typedef struct registers {
	uint64_t gs;
	uint64_t fs;
	uint64_t es;
	uint64_t ds;
	uint64_t cr2;
	uint64_t stub; // to make the struct aligned
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t rbp;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
	uint64_t err_type;
	uint64_t err_code;
	uint64_t rip;
	uint64_t cs;
	uint64_t flags;
	uint64_t rsp;
	uint64_t ss;
} registers_t;

typedef uint64_t stmm_t[2];
typedef uint64_t xmm_t[2];

typedef struct fpu_regs {
	uint16_t fcw;
	uint16_t fsw;
	uint8_t ftw;
	uint8_t reserved1;
	uint16_t fop;
	uint64_t fip;
	uint64_t fdp;
	uint32_t mxcsr;
	uint32_t mxcsr_mask;
	stmm_t stmms[8];
	xmm_t xmms[16];
	uint8_t reserved2[96];
} __attribute__((packed, aligned(16))) arch_fpu_t;

typedef struct acontext {
	arch_fpu_t fpu;
	registers_t frame;
	void *tls_base;
} __attribute__((aligned(16))) acontext_t;

#if defined(__KERNEL__) || defined(__MODULE__)

// arch specific functions
void arch_set_kernel_stack(uintptr_t stack);
void arch_set_tls(void *tls);
int arch_registers_save(registers_t *registers);
void arch_registers_load(registers_t *registers);
void arch_registers_dump(registers_t *registers);
void arch_registers_stacktrace(registers_t *registers);

static inline uintptr_t arch_fault_get_addr(registers_t *fault) {
	return fault->cr2;
}

long arch_fault_get_prot(registers_t *fault);

static inline void arch_fpu_save(arch_fpu_t *fpu) {
	asm volatile("fxsave64 %0" : "=m" (*fpu));
}
static inline void arch_fpu_load(arch_fpu_t *fpu) {
	asm volatile("fxrstor64 %0" : : "m" (*fpu));
}

static inline void arch_fpu_init(arch_fpu_t *fpu) {
	memset(fpu, 0, sizeof(arch_fpu_t));
	fpu->fcw     = 0x037f;
	fpu->mxcsr   = 0x1F80;
}

static inline void arch_fpu_enable(void) {
	asm volatile("clts");
}

static inline void arch_fpu_disable(void) {
	asm volatile("movq %%cr0, %%rax\n"
			"or $0x8, %%rax\n"
			"movq %%rax, %%cr0" : : : "rax", "memory");
}

/**
 * @brief initalize registers with sane values
 * @param registers theregisters to initalize
 * @param stack the top of the stack
 * @param start the start of execution
 * @param userspace is this context a userspace one
 */
void arch_registers_init(registers_t *registers, void *stack, void *start, int userspace);

/**
 * @brief check if a specfied context is in userspace
 * @param registers the context to check
 * @return 1 of if userspace 0 if kernel space
 */
int arch_registers_is_userspace(registers_t *registers);

void init_root_bus(void);
void init_timer(void);
void init_arch_irq();
void enable_sse(void);
int arch_shutdown(int flags);
#endif


#define ARG0_REG(registers) (registers).rax
#define ARG1_REG(registers) (registers).rdi
#define ARG2_REG(registers) (registers).rsi
#define ARG3_REG(registers) (registers).rdx
#define ARG4_REG(registers) (registers).rcx
#define ARG5_REG(registers) (registers).r8
#define ARG6_REG(registers) (registers).r9
#define RET_REG(registers)  (registers).rax
#define SP_REG(registers)   (registers).rsp
#define PC_REG(registers)   (registers).rip

typedef int intrnum_t;

#define IRQ_CHIP_APIC 0x01
#define IRQ_CHIP_PIC  0x02

#endif
