#ifndef MEMSEG_H
#define MEMSEG_H

#include <kernel/scheduler.h>
#include <stdint.h>

memseg *memseg_map(process *proc, uint64_t address,size_t size,uint64_t flags);
void memseg_unmap(process *proc,memseg *seg);
void memseg_clone(process *parent,process *child,memseg *seg);
void memeseg_chflag(process *proc,memseg *seg,uint64_t flags);

#endif