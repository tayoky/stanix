#ifndef ARCH_H
#define ARCH_H

#include <stdint.h>
#include "panic.h"
#include "scheduler.h"

//arch specific functions

void context_switch(process *old,process *new);
void context_save(fault_frame *context);
void context_load(fault_frame *context);

#endif