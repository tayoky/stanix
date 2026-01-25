#ifndef ARCH_H
#define ARCH_H

#include <stdint.h>

typedef struct fault_frame_t{
	uint64_t x0;
	uint64_t x1;
	uint64_t x2;
	uint64_t x3;
	uint64_t x4;
	uint64_t x5;
	uint64_t x6;
	uint64_t x7;
	uint64_t x8;
	uint64_t x9;
	uint64_t x10;
	uint64_t x11;
	uint64_t x12;
	uint64_t x13;
	uint64_t x14;
	uint64_t x15;
	uint64_t x16;
	uint64_t x17;
	uint64_t x18;
	uint64_t x19;
	uint64_t x20;
	uint64_t x21;
	uint64_t x22;
	uint64_t x23;
	uint64_t x24;
	uint64_t x25;
	uint64_t x26;
	uint64_t x27;
	uint64_t x28;
	uint64_t x29;
	uint64_t x30;
	uint64_t err_code;
	uint64_t err_type;
} fault_frame_t;

#include <kernel/scheduler.h>

//arch specific functions

void context_switch(task_t *old,task_t *new);

typedef struct arch_specific {

} arch_specific;

#define ARG0_REG(frame) ( frame ).x4
#define ARG1_REG(frame) ( frame ).x0
#define ARG2_REG(frame) ( frame ).x1
#define ARG3_REG(frame) ( frame ).x2
#define ARG4_REG(frame) ( frame ).x3
#define RET_REG(frame)  ( frame ).x0
#define SP_REG(frame)   ( frame ).x30

#endif