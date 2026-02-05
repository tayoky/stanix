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
	int (*msync)(struct vmm_seg *seg, uintptr_t start, uintptr_t end, int flags);
	int (*fault)(struct vmm_seg *seg, uintptr_t addr, long prot);
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
	off_t offset;
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

/**
 * @brief mmap memory and create a segment for it
 */
int vmm_map(uintptr_t address, size_t size, long prot, int flags, vfs_fd_t *fd, off_t offset, vmm_seg_t **seg);

/**
 * @brief clone a segment from the parent to the child
 * @param parent the parent to clone the segment from
 * @param child the child to clone the segment to
 * @param seg the segment to clone
 */
void vmm_clone(process_t *parent, process_t *child, vmm_seg_t *seg);

/**
 * @brief unmap a segment
 * @param seg the segment to unmap
 */
void vmm_unmap(vmm_seg_t *seg);

/**
 * @brief unmap all segment in a range, spliting them if necessary
 * @param start the start of the range
 * @param end the end of the range
 * @return 0 on success or error code on failure
 */
int vmm_unmap_range(uintptr_t start, uintptr_t end);

/**
 * @brief change the protections flags of a segment
 * @param seg the segment to change the protection of
 * @param prot the new protection flags
 * @return 0 on succes or error code on failure
 */
int vmm_chprot(vmm_seg_t *seg, long prot);

/**
 * @brief split a segment
 * @param seg the segment to split
 * @param cur where to split (must be in the segment's bounds)
 * @param out_seg return a pointer to the newly created sgement (after the current one)
 * @return 0 on succes or error code on failure
 */
int vmm_split(vmm_seg_t *seg, uintptr_t cut, vmm_seg_t **out_seg);

/**
 * @brief flush a segment to the disk/device
 * @param seg the segment to flush
 * @param start the start of the region to flush (must be in the segment's bounds)
 * @param end the end of the region to flush (must be in the segment's bounds)
 * @param flags the flags to flush with
 * @return 0 on succes or error code on failure
 */
int vmm_sync(vmm_seg_t *seg, uintptr_t start, uintptr_t end, int flags);

#endif
