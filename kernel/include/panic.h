#ifndef PANIC_H
#define PANIC_H
#include <stdint.h>

//todo move that to arch.h
typedef struct{
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

void panic(const char *error,fault_frame *fault);

#ifndef NULL
#define NULL (void *)0
#endif

#endif