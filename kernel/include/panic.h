#ifndef PANIC_H
#define PANIC_H
#include <stdint.h>

typedef struct{
	uint64_t cr3;
	uint64_t cr2;
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rip;
	uint64_t rbp;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t rbx;
	uint64_t rax;
} regs;


void panic(const char *error,regs registers_state);

#endif