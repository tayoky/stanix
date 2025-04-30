#ifndef ARCH_H
#define ARCH_H

#include "gdt.h"
#include "idt.h"
#include "tss.h"

#include <stdint.h>

typedef struct fault_frame{
	uint32_t cr2;
	uint32_t cr3;
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t ebp;
	uint32_t err_type;
	uint32_t err_code;
	uint32_t rip;
	uint32_t cs;
	uint32_t flags;
	uint32_t esp;
	uint32_t ss;
} fault_frame;

typedef struct arch_specific {
	gdt_segment gdt[6];
	GDTR gdtr;
	idt_gate idt[256];
	IDTR idtr;
	TSS tss;
	uint32_t hPDP[8];
} arch_specific ;

#include <kernel/scheduler.h>

//arch specific functions

void context_switch(process *old,process *new);

void init_timer();

#define ARG0_REG(frame) ( frame ).eax
#define ARG1_REG(frame) ( frame ).edi
#define ARG2_REG(frame) ( frame ).esi
#define ARG3_REG(frame) ( frame ).edx
#define ARG4_REG(frame) ( frame ).ecx
#define RET_REG(frame)  ( frame ).eax
#define SP_REG(frame)   ( frame ).esp

#endif