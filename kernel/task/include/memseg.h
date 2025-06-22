#ifndef MEMSEG_H
#define MEMSEG_H

#include <kernel/scheduler.h>
#include <stdint.h>
#include <stddef.h>

typedef struct memseg_struct {
	uintptr_t addr;
	size_t size;
	struct memseg_struct *next;
	struct memseg_struct *prev;
	uint64_t flags;
} memseg;


memseg *memseg_map(process *proc, uintptr_t address,size_t size,uint64_t flags);
void memseg_unmap(process *proc,memseg *seg);
void memseg_clone(process *parent,process *child,memseg *seg);
void memseg_chflag(process *proc,memseg *seg,uint64_t flags);

#endif