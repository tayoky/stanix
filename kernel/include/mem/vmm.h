#ifndef KERNEL_VMM_H
#define KERNEL_VMM_H

#include <kernel/spinlock.h>
#include <kernel/rwlock.h>
#include <kernel/list.h>
#include <kernel/mmu.h>
#include <stdint.h>
#include <stddef.h>

struct vfs_fd;
struct vmm_seg;
struct process;

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
	spinlock_t lock;
} vmm_seg_t;

#define VMM_FLAG_ANONYMOUS 0x01
#define VMM_FLAG_PRIVATE   0x02
#define VMM_FLAG_SHARED    0x04
#define VMM_FLAG_IO        0x08
#define VMM_SIZE(seg) (seg->end - seg->start)
#define VMM_FLAG_SYNC  0x1
#define VMM_FLAG_ASYNC 0x2

typedef struct vmm_space {
	addrspace_t addrspace;
	list_t segs;
	rwlock_t lock;
	size_t total_size;
	size_t peak_size;
	size_t private_size;
	size_t shared_size;
	size_t file_size;
	size_t anon_size;
	atomic_size_t page_faults;
} vmm_space_t;

/**
 * @brief report a seg fault to vmm_seg
 * @param addr the faulting address
 * @param prot the type of fault
 * @return 1 if the fault is handled or 0 if not
 */
int vmm_fault_report(uintptr_t addr, int prot);

/**
 * @brief initalize a new vmm address space
 * @param space the address space to initalize
 */
void vmm_init_space(vmm_space_t *space);

/**
 * @brief destroy a vmm address space
 * @param space the address space to destroy
 */
void vmm_destroy_space(vmm_space_t *space);


/**
 * @brief get the current address space
 * @return the current address space
 */
vmm_space_t *vmm_get_current_space(void);

/**
 * @brief mmap memory and create a segment for it into a specfied address space
 */
vmm_seg_t *vmm_space_map(vmm_space_t *space, uintptr_t address, size_t size, long prot, int flags, struct vfs_fd *fd, off_t offset);

/**
 * @brief mmap memory and create a segment for it
 */
static inline vmm_seg_t *vmm_map(uintptr_t address, size_t size, long prot, int flags, struct vfs_fd *fd, off_t offset) {
	return vmm_space_map(vmm_get_current_space(), address, size, prot, flags, fd, offset);
}

/**
 * @brief clone a vmm address space from the parent to the child
 * @param parent the parent address space to clone from
 * @param child the child address space to clone to
 */
int vmm_clone(vmm_space_t *parent, vmm_space_t *child);

/**
 * @brief unmap a segment in a specified address space
 * @param space the address space in which the segment is
 * @param seg the segment to unmap
 */
void vmm_space_unmap(vmm_space_t *space, vmm_seg_t *seg);

/**
 * @brief unmap a segment
 * @param seg the segment to unmap
 */
static inline void vmm_unmap(vmm_seg_t *seg) {
	vmm_space_unmap(vmm_get_current_space(), seg);
}

/**
 * @brief unmap all segment in a range, spliting them if necessary in a specifed address space
 * @param space the address space in which the range is
 * @param start the start of the range
 * @param end the end of the range
 * @return 0 on success or error code on failure
 */
int vmm_space_unmap_range(vmm_space_t *space, uintptr_t start, uintptr_t end);

/**
 * @brief unmap all segment in a range, spliting them if necessary
 * @param start the start of the range
 * @param end the end of the range
 * @return 0 on success or error code on failure
 */
static inline int vmm_unmap_range(uintptr_t start, uintptr_t end) {
	return vmm_space_unmap_range(vmm_get_current_space(), start, end);
}

/**
 * @brief unmap all segments in a specified address space
 * @param space the address space in which to unmap segments
 */
void vmm_space_unmap_all(vmm_space_t *space);

/**
 * @brief unmap all segments
 */
static inline void vmm_unmap_all(void) {
	vmm_space_unmap_all(vmm_get_current_space());
}

/**
 * @brief change the protections flags of a segment in a specfied address space
 * @param space the address space in which the segment is
 * @param seg the segment to change the protection of
 * @param prot the new protection flags
 * @return 0 on success or error code on failure
 */
int vmm_space_chprot(vmm_space_t *space, vmm_seg_t *seg, long prot);

/**
 * @brief change the protections flags of a segment
 * @param seg the segment to change the protection of
 * @param prot the new protection flags
 * @return 0 on success or error code on failure
 */
static inline int vmm_chprot(vmm_seg_t *seg, long prot) {
	return vmm_space_chprot(vmm_get_current_space(), seg, prot);
}

/**
 * @brief change the protections flags of segments in a range, spliting them if necessary in a specfied address space
 * @param space the address space in which the segment is
 * @param start the start of the range
 * @param end the end of the range
 * @param prot the new protection flags
 * @return 0 on success or error code on failure
 */
int vmm_space_chprot_range(vmm_space_t *space, uintptr_t start, uintptr_t end, long prot);

/**
 * @brief change the protections flags of segments in a range, spliting them if necessary
 * @param start the start of the range
 * @param end the end of the range
 * @param prot the new protection flags
 * @return 0 on success or error code on failure
 */
static inline int vmm_chprot_range(uintptr_t start, uintptr_t end, long prot) {
	return vmm_space_chprot_range(vmm_get_current_space(), start, end, prot);
}

/**
 * @brief split a segment in a specfied address space
 * @param space the address space in which the segment is
 * @param seg the segment to split
 * @param cur where to split (must be in the segment's bounds)
 * @param out_seg return a pointer to the newly created sgement (after the current one)
 * @return 0 on success or error code on failure
 */
int vmm_space_split(vmm_space_t *space, vmm_seg_t *seg, uintptr_t cut, vmm_seg_t **out_seg);

/**
 * @brief split a segment in the current address space
 * @param seg the segment to split
 * @param cur where to split (must be in the segment's bounds)
 * @param out_seg return a pointer to the newly created sgement (after the current one)
 * @return 0 on success or error code on failure
 */
static inline int vmm_split(vmm_seg_t *seg, uintptr_t cut, vmm_seg_t **out_seg) {
	return vmm_space_split(vmm_get_current_space(), seg, cut, out_seg);
}

/**
 * @brief flush a segment in a specfied address space to the disk/device
 * @param space the address space in which the segment is
 * @param seg the segment to flush
 * @param start the start of the region to flush (must be in the segment's bounds)
 * @param end the end of the region to flush (must be in the segment's bounds)
 * @param flags the flags to flush with
 * @return 0 on success or error code on failure
 */
int vmm_space_sync(vmm_space_t *space, vmm_seg_t *seg, uintptr_t start, uintptr_t end, int flags);

/**
 * @brief flush a segment to the disk/device
 * @param seg the segment to flush
 * @param start the start of the region to flush (must be in the segment's bounds)
 * @param end the end of the region to flush (must be in the segment's bounds)
 * @param flags the flags to flush with
 * @return 0 on success or error code on failure
 */
static inline int vmm_sync(vmm_seg_t *seg, uintptr_t start, uintptr_t end, int flags) {
	return vmm_space_sync(vmm_get_current_space(), seg, start, end, flags);
}

/**
 * @brief flush segments in a spcefied address space in a range to the disk/device
 * @param start the start of the range
 * @param end the end of the range
 * @param flags the flags to flush with
 * @return 0 on success or error code on failure
 */
int vmm_space_sync_range(vmm_space_t *space, uintptr_t start, uintptr_t end, int flags);

/**
 * @brief flush segments in a range to the disk/device
 * @param start the start of the range
 * @param end the end of the range
 * @param flags the flags to flush with
 * @return 0 on success or error code on failure
 */
static inline int vmm_sync_range(uintptr_t start, uintptr_t end, int flags) {
	return vmm_space_sync_range(vmm_get_current_space(), start, end, flags);
}

void init_vmm(void);

#endif
