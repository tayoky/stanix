#ifndef MEMSEG_H
#define MEMSEG_H

#include "scheduler.h"
#include <stdint.h>

void memseg_map(process *proc, uint64_t address,size_t size,uint64_t flags);
void memseg_unmap(process *proc,memseg *seg);

#endif