#ifndef ARCH_H
#define ARCH_H

#include "gdt.h"
#include "idt.h"
#include "tss.h"

#include <stdint.h>

typedef struct fault_frame{
	uint64_t cr2;
	uint64_t cr3;
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
} fault_frame;

typedef struct arch_specific {
	gdt_segment gdt[7];
	GDTR gdtr;
	idt_gate idt[256];
	IDTR idtr;
	TSS tss;
	uint64_t hPDP[8];
} arch_specific ;

#include <kernel/scheduler.h>

//arch specific functions
void set_kernel_stack(uintptr_t stack);
void context_switch(uintptr_t new_rsp,uintptr_t *old_rsp);

void init_timer(void);
void enable_sse(void);

#define ARG0_REG(frame) ( frame ).rax
#define ARG1_REG(frame) ( frame ).rdi
#define ARG2_REG(frame) ( frame ).rsi
#define ARG3_REG(frame) ( frame ).rdx
#define ARG4_REG(frame) ( frame ).rcx
#define ARG5_REG(frame) ( frame ).r8
#define RET_REG(frame)  ( frame ).rax
#define SP_REG(frame)   ( frame ).rsp

#endif