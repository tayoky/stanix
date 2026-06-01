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

typedef struct arch_specific {
	gdt_segment gdt[7];
	GDTR gdtr;
	idt_gate idt[256];
	IDTR idtr;
	TSS tss;
	uint64_t hPDP[8];
} arch_specific;

typedef struct acontext {
	char sse[512];
	registers_t frame;
	uint64_t fs_base;
} __attribute__((aligned(16))) acontext_t;

// arch specific functions
void arch_set_kernel_stack(uintptr_t stack);
int arch_save_context(acontext_t *context);
void arch_load_context(acontext_t *context);
void arch_registers_dump(registers_t *registers);
void arch_registers_stacktrace(registers_t *registers);
uintptr_t arch_fault_get_addr(registers_t *fault);
long arch_fault_get_prot(registers_t *fault);

/**
 * @brief check if a specfied context is in userspace
 * @param frame the context to check
 * @return 1 of if userspace 0 if kernel space
 */
int arch_registers_is_userspace(registers_t *frame);

void init_timer(void);
void arch_set_tls(void *tls);
void enable_sse(void);
int arch_shutdown(int flags);

#define ARG0_REG(frame) (frame).rax
#define ARG1_REG(frame) (frame).rdi
#define ARG2_REG(frame) (frame).rsi
#define ARG3_REG(frame) (frame).rdx
#define ARG4_REG(frame) (frame).rcx
#define ARG5_REG(frame) (frame).r8
#define ARG6_REG(frame) (frame).r9
#define RET_REG(frame)  (frame).rax
#define SP_REG(frame)   (frame).rsp
#define PC_REG(frame)   (frame).rip

typedef int irqnum_t;

typedef struct irq_chip {
	void (*mask)(irqnum_t irq_num);
	void (*unmask)(irqnum_t irq_num);
	void (*eoi)(irqnum_t irq_num);
	void (*register_handler)(irqnum_t irq_num, void *handler, void *data);
	irqnum_t (*hirq2irq)(int hirq);
	const char *name;
	int type;
} irq_chip_t;

extern irq_chip_t *irq_chip;

#define IRQ_CHIP_APIC 0x01
#define IRQ_CHIP_PIC  0x02

#endif
