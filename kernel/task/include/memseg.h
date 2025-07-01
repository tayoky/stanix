#ifndef MEMSEG_H
#define MEMSEG_H

#include <kernel/scheduler.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stddef.h>

typedef struct memseg_struct {
	uintptr_t addr;
	size_t size;
	uint64_t prot;
	long flags;
	long ref_count;
	void (*unmap)(struct memseg_struct *);
} memseg;

memseg *memseg_create(process *proc,uintptr_t address,size_t size,uint64_t prot,int flags);
memseg *memseg_map(process *proc, uintptr_t address,size_t size,uint64_t prot,int flags);
void memseg_unmap(process *proc,memseg *seg);
void memseg_clone(process *parent,process *child,memseg *seg);
void memseg_chflag(process *proc,memseg *seg,uint64_t flags);

#endif