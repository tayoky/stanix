#ifndef _KERNEL_X86_64_H
#define _KERNEL_X86_64_H

#include "asm.h"
#include "cmos.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "paging.h"
#include "pic.h"
#include "pit.h"
#include "port.h"
#include "serial.h"
#include "tss.h"
#include <sys/shutdown.h>
#include <stdint.h>

//any change here must be replicataed in interrupt handler
//and context switch
typedef struct fault_frame{
	uint64_t gs;
	uint64_t fs;
	uint64_t es;
	uint64_t ds;
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

typedef struct acontext {
	char sse[512];
	fault_frame frame;
	uint64_t fs_base;
} acontext;

//arch specific functions
void set_kernel_stack(uintptr_t stack);
int save_context(acontext *context);
void load_context(acontext *context);
uintptr_t get_ptr_context(fault_frame *fault);
long arch_get_prot_fault(fault_frame *fault);

/// @brief check if a specfied context is in userspace
/// @param frame the context to check
/// @return 1 of if userspace 0 if kernel space
int is_userspace(fault_frame *frame);

void init_timer(void);
void set_tls(void *tls);
void enable_sse(void);
int arch_shutdown(int flags);

#define ARG0_REG(frame) ( frame ).rax
#define ARG1_REG(frame) ( frame ).rdi
#define ARG2_REG(frame) ( frame ).rsi
#define ARG3_REG(frame) ( frame ).rdx
#define ARG4_REG(frame) ( frame ).rcx
#define ARG5_REG(frame) ( frame ).r8
#define RET_REG(frame)  ( frame ).rax
#define SP_REG(frame)   ( frame ).rsp
#define PC_REG(frame)   ( frame ).rip

#define PIC_APIC 0x01
#define PIC_PIC  0x02

#endif