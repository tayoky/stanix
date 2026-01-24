#ifndef _KERNEL_VMM_H
#define _KERNEL_VMM_H

#include <kernel/scheduler.h>
#include <kernel/list.h>
#include <kernel/mmu.h>
#include <stdint.h>
#include <stddef.h>

struct vfs_fd;
struct vmm_seg;

typedef struct vmm_ops {
	void (*open)(struct vmm_seg *seg);
	void (*close)(struct vmm_seg *seg);
	int (*can_mprotect)(struct vmm_seg *seg, long prot);
	int (*can_split)(struct vmm_seg *seg, uintptr_t cut);
} vmm_ops_t;

typedef struct vmm_seg {
	list_node_t node;
	uintptr_t start;
	uintptr_t end;
	long prot;
	long flags;
	struct vfs_fd *fd;
	void *private_data;
	vmm_ops_t *ops;
} vmm_seg_t;

#define VMM_FLAG_ANONYMOUS 0x01
#define VMM_FLAG_PRIVATE   0x02
#define VMM_FLAG_SHARED    0x04
#define VMM_FLAG_IO        0x08
#define VMM_SIZE(seg) (seg->end - seg->start)

/**
 * @brief report a seg fault to vmm_seg
 * @param addr the faulting address
 * @param prot the type of fault
 * @return 1 if the fault is handled or 0 if not
 */
int vmm_fault_report(uintptr_t addr, int prot);

vmm_seg_t *vmm_create_seg(process_t *proc, uintptr_t address, size_t size, long prot, int flags);
int vmm_map(process_t *proc, uintptr_t address, size_t size, long prot, int flags, vfs_fd_t *fd, off_t offset, vmm_seg_t **seg);
void vmm_unmap(process_t *proc, vmm_seg_t *seg);
void vmm_clone(process_t *parent, process_t *child, vmm_seg_t *seg);
int vmm_chprot(process_t *proc, vmm_seg_t *seg, long prot);

#endif
