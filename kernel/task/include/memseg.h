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
	void *private_data;
} memseg_t;

memseg_t *memseg_create(process_t *proc,uintptr_t address,size_t size,uint64_t prot,int flags);
int memseg_map(process_t *proc, uintptr_t address,size_t size,uint64_t prot,int flags,vfs_fd_t *fd,off_t offset,memseg_t **seg);
void memseg_unmap(process_t *proc,memseg_t *seg);
void memseg_clone(process_t *parent,process_t *child,memseg_t *seg);
void memseg_chflag(process_t *proc,memseg_t *seg,uint64_t flags);

#endif