#include <kernel/kernel.h>
#include <kernel/list.h>
#include <kernel/mmu.h>
#include <kernel/pmm.h>
#include <kernel/print.h>
#include <kernel/scheduler.h>
#include <kernel/signal.h>
#include <kernel/slab.h>
#include <kernel/string.h>
#include <kernel/vmm.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

static slab_cache_t vmm_seg_slab;

static int vmm_space_raw_unmap_range(vmm_space_t *space, uintptr_t start, uintptr_t end);

static int vmm_seg_constructor(slab_cache_t *cache, void *data) {
	(void)cache;
	vmm_seg_t *seg = data;
	memset(seg, 0, sizeof(vmm_seg_t));
	return 0;
}

void init_vmm(void) {
	kstatusf("init vmm ... ");
	slab_init(&vmm_seg_slab, sizeof(vmm_seg_t), "vmm-segments");
	vmm_seg_slab.constructor = vmm_seg_constructor;
	kok();
}

static void vmm_cow(vmm_seg_t *seg, uintptr_t vpage) {
	uintptr_t phys  = mmu_virt2phys((void *)vpage);
	if (atomic_load(&pmm_page_info(phys)->ref_count) <= 1) {
		// other processes already copied
		mmu_set_flags(get_current_proc()->vmm_space.addrspace, vpage, seg->prot);
	} else {
		// we need to copy
		uintptr_t new_page = pmm_dup_page(phys);
		if (new_page == PAGE_INVALID) {
			// not looking good
			send_sig_task(get_current_task(), SIGBUS);
			return;
		}
		pmm_release_page(phys);
		mmu_map_page(get_current_proc()->vmm_space.addrspace, new_page, vpage, seg->prot);
	}
	return;
}

static int vmm_handle_fault(vmm_seg_t *seg, uintptr_t addr, int prot) {
	spinlock_acquire(&seg->lock);
	uintptr_t vpage = PAGE_ALIGN_DOWN((uintptr_t)addr);

	if (seg->ops && seg->ops->fault) {
		if (seg->ops->fault(seg, addr, prot)) {
			spinlock_release(&seg->lock);
			return 1;
		}
	}

	if ((seg->flags & VMM_FLAG_PRIVATE) && (seg->prot & MMU_FLAG_WRITE) && prot == MMU_FLAG_WRITE) {
		// we failed a write but we have write perm
		// it's CoW
		vmm_cow(seg, vpage);
		if (vpage + PAGE_SIZE < seg->end) {
			vmm_cow(seg, vpage + PAGE_SIZE);
		}
		spinlock_release(&seg->lock);
		return 1;
	}
	spinlock_release(&seg->lock);
	return 0;
}

int vmm_fault_report(uintptr_t addr, int prot) {
	if (!get_current_proc()) return 0;
	int interrupt_save;
	rwlock_acquire_read(&get_current_proc()->vmm_space.lock, &interrupt_save);
	atomic_fetch_add(&get_current_proc()->vmm_space.page_faults, 1);
	foreach (node, &get_current_proc()->vmm_space.segs) {
		vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
		if (seg->start > addr) break;
		if (seg->end > addr) {
			// we found a seg to report to
			int ret = vmm_handle_fault(seg, addr, prot);
			rwlock_release_read(&get_current_proc()->vmm_space.lock, &interrupt_save);
			return ret;
		}
	}
	rwlock_release_read(&get_current_proc()->vmm_space.lock, &interrupt_save);
	return 0;
}

void vmm_init_space(vmm_space_t *space) {
	memset(space, 0, sizeof(vmm_space_t));
	space->addrspace = mmu_create_addr_space();
}

void vmm_destroy_space(vmm_space_t *space) {
	vmm_space_unmap_all(space);
	mmu_delete_addr_space(space->addrspace);
	list_destroy(&space->segs);
}

vmm_space_t *vmm_get_current_space(void) {
	return &get_current_proc()->vmm_space;
}

int vmm_space_split(vmm_space_t *space, vmm_seg_t *seg, uintptr_t cut, vmm_seg_t **out_seg) {
	spinlock_acquire(&seg->lock);
	if (cut <= seg->start || cut >= seg->end) {
		spinlock_release(&seg->lock);
		return -EINVAL;
	}
	if (seg->ops && seg->ops->can_split) {
		int ret = seg->ops->can_split(seg, cut);
		if (ret < 0) {
			spinlock_release(&seg->lock);
			return ret;
		}
	}

	vmm_seg_t *new_seg    = slab_alloc(&vmm_seg_slab);
	new_seg->prot         = seg->prot;
	new_seg->flags        = seg->flags;
	new_seg->start        = cut;
	new_seg->end          = seg->end;
	new_seg->ops          = seg->ops;
	new_seg->private_data = seg->private_data;
	new_seg->fd           = vfs_dup(seg->fd);

	seg->end = cut;

	if (seg->ops && seg->ops->open) {
		seg->ops->open(new_seg);
	}

	list_add_after(&space->segs, &seg->node, &new_seg->node);

	if (out_seg) *out_seg = new_seg;
	spinlock_release(&seg->lock);
	return 0;
}

static int vmm_space_raw_create_seg(vmm_space_t *space, uintptr_t address, size_t size, long prot, int flags, vmm_seg_t **seg) {
	vmm_seg_t *prev = NULL;
	if (address) {
		uintptr_t end = address + size;

		// first remove any overlaping mapping
		int ret = vmm_space_raw_unmap_range(space, address, end);
		if (ret < 0) return ret;

		// then find where we need to insert
		foreach (node, &space->segs) {
			vmm_seg_t *current = container_of(node, vmm_seg_t, node);
			if (current->start >= end) {
				break;
			}
			prev = current;
		}
	} else {
		// we need to page align everything
		size = PAGE_ALIGN_UP(size);

		// no address ? we need to find one ourself
		foreach (node, &space->segs) {
			vmm_seg_t *current = container_of(node, vmm_seg_t, node);
			vmm_seg_t *next    = container_of(node->next, vmm_seg_t, node);
			if (!next || next->start - current->end >= size) {
				address = current->end;
				prev    = current;
				break;
			}
		}

		if (!address) {
			// hardcoded userspace start
			address = PAGE_SIZE * 12;
			prev    = NULL;
		}
	}

	vmm_seg_t *new_seg = slab_alloc(&vmm_seg_slab);
	new_seg->start     = address;
	new_seg->end       = address + size;
	new_seg->prot      = prot;
	new_seg->flags     = flags;
	*seg               = new_seg;

	list_add_after(&space->segs, prev ? &prev->node : NULL, &new_seg->node);

	return 0;
}

static int vmm_space_raw_map(vmm_space_t *space, uintptr_t address, size_t size, long prot, int flags, vfs_fd_t *fd, off_t offset, vmm_seg_t **seg) {
	// we need size to be aligned
	size = PAGE_ALIGN_UP(size);

	vmm_seg_t *new_seg;
	int ret = vmm_space_raw_create_seg(space, address, size, prot, flags, &new_seg);
	if (ret < 0) return ret;

	// kdebugf("map %p size : %lx\n", new_seg->start, size);
	if (flags & VMM_FLAG_ANONYMOUS) {
		fd = NULL;
		for (uintptr_t addr = new_seg->start; addr < new_seg->end; addr += PAGE_SIZE) {
			// TODO : handle error from pmm_allocate_page
			uintptr_t page;
			if (flags & VMM_FLAG_SHARED) {
				page = pmm_allocate_page();
				memset(mmu_phys2virt(page), 0, PAGE_SIZE);
				mmu_map_page(space->addrspace, page, addr, prot);
			} else {
				page = pmm_get_zero_page();
				mmu_map_page(space->addrspace, page, addr, prot & ~MMU_FLAG_WRITE);
			}
		}
	} else if (fd) {
		ret = vfs_mmap(fd, offset, new_seg);
	} else {
		ret = -EINVAL;
	}

	if (ret < 0) {
		list_remove(&space->segs, &new_seg->node);
		slab_free(new_seg);
	} else {
		if (seg) *seg = new_seg;
		if (fd) {
			new_seg->fd     = vfs_dup(fd);
			new_seg->offset = offset;
			space->file_size += VMM_SIZE(new_seg);
		} else {
			space->anon_size += VMM_SIZE(new_seg);
		}
		if (flags & VMM_FLAG_PRIVATE) {
			space->private_size += VMM_SIZE(new_seg);
		} else {
			space->shared_size += VMM_SIZE(new_seg);
		}
		space->total_size += VMM_SIZE(new_seg);
		if (space->total_size > space->peak_size) {
			space->peak_size = space->total_size;
		}
	}
	return ret;
}

int vmm_space_map(vmm_space_t *space, uintptr_t address, size_t size, long prot, int flags, struct vfs_fd *fd, off_t offset, vmm_seg_t **seg) {
	int interrupt_save;
	rwlock_acquire_write(&space->lock, &interrupt_save);
	int ret = vmm_space_raw_map(space, address, size, prot, flags, fd, offset, seg);
	rwlock_release_write(&space->lock, &interrupt_save);
	return ret;
}

int vmm_space_chprot(vmm_space_t *space, vmm_seg_t *seg, long prot) {
	spinlock_acquire(&seg->lock);
	if (seg->ops && seg->ops->can_mprotect) {
		int ret = seg->ops->can_mprotect(seg, prot);
		if (ret < 0) {
			spinlock_release(&seg->lock);
			return ret;
		}
	}
	seg->prot = prot;

	for (uintptr_t addr = seg->start; addr < seg->end; addr += PAGE_SIZE) {
		uintptr_t phys = mmu_space_virt2phys(space->addrspace, (void *)addr);
		if ((seg->flags & VMM_FLAG_PRIVATE) && pmm_page_info(phys)->ref_count > 1) {
			// we are doing CoW
			mmu_set_flags(space->addrspace, addr, prot & ~MMU_FLAG_WRITE);
		} else {
			mmu_set_flags(space->addrspace, addr, prot);
		}
	}
	spinlock_release(&seg->lock);
	return 0;
}

static int vmm_space_raw_chprot_range(vmm_space_t *space, uintptr_t start, uintptr_t end, long prot) {
	foreach (node, &space->segs) {
		vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
		if (seg->end <= start) continue;
		if (seg->start >= end) break;
		if (start > seg->start) {
			int ret = vmm_space_split(space, seg, start, NULL);
			if (ret < 0) return ret;

			// on the next iteration we will land on the newly created segment
			continue;
		}
		if (end < seg->end) {
			int ret = vmm_space_split(space, seg, end, NULL);
			if (ret < 0) return ret;
		}
		int ret = vmm_space_chprot(space, seg, prot);
		if (ret < 0) return ret;
	}

	return 0;
}

int vmm_space_chprot_range(vmm_space_t *space, uintptr_t start, uintptr_t end, long prot) {
	int interrupt_save;
	rwlock_acquire_read(&space->lock, &interrupt_save);
	int ret = vmm_space_raw_chprot_range(space, start, end, prot);
	rwlock_release_read(&space->lock, &interrupt_save);
	return ret;
}

static void vmm_space_raw_unmap(vmm_space_t *space, vmm_seg_t *seg) {
	spinlock_acquire(&seg->lock);
	list_remove(&space->segs, &seg->node);
	space->total_size -= VMM_SIZE(seg);
	if (seg->fd) {
		space->file_size -= VMM_SIZE(seg);
	} else {
		space->anon_size -= VMM_SIZE(seg);
	}
	if (seg->flags & VMM_FLAG_PRIVATE) {
		space->private_size -= VMM_SIZE(seg);
	} else {
		space->shared_size -= VMM_SIZE(seg);
	}

	// kdebugf("unmap %p to %p\n", seg->start, seg->end);

	// flush shared mappings so we don't lost changes
	if ((seg->flags & VMM_FLAG_SHARED) && seg->ops && seg->ops->msync) {
		seg->ops->msync(seg, seg->start, seg->end, 0);
	}

	if (seg->ops && seg->ops->close) {
		seg->ops->close(seg);
	}
	vfs_close(seg->fd);

	// IO cannot be allocated/freed using the PMM
	// so do not free them
	// it's the driver job to do it
	if (!(seg->flags & VMM_FLAG_IO)) {
		for (uintptr_t addr = seg->start; addr < seg->end; addr += PAGE_SIZE) {
			uintptr_t page = mmu_virt2phys((void *)addr);
			if (page == PAGE_INVALID) continue;
			pmm_release_page(page);
		}
	}

	for (uintptr_t addr = seg->start; addr < seg->end; addr += PAGE_SIZE) {
		mmu_unmap_page(space->addrspace, addr);
	}
	slab_free(seg);
}

void vmm_space_unmap(vmm_space_t *space, vmm_seg_t *seg) {
	int interrupt_save;
	rwlock_acquire_write(&space->lock, &interrupt_save);
	vmm_space_raw_unmap(space, seg);
	rwlock_release_write(&space->lock, &interrupt_save);
}

static int vmm_space_raw_unmap_range(vmm_space_t *space, uintptr_t start, uintptr_t end) {
	vmm_seg_t *seg = (vmm_seg_t *)space->segs.first_node;
	for (vmm_seg_t *next; seg; seg = next) {
		next = (vmm_seg_t *)seg->node.next;

		if (seg->end <= start) continue;
		if (seg->start >= end) break;
		if (start > seg->start) {
			int ret = vmm_space_split(space, seg, start, NULL);
			if (ret < 0) return ret;

			// on the next iteration we will land on the newly created segment
			continue;
		}
		if (end < seg->end) {
			int ret = vmm_space_split(space, seg, end, NULL);
			if (ret < 0) return ret;
		}
		vmm_space_raw_unmap(space, seg);
	}
	return 0;
}

int vmm_space_unmap_range(vmm_space_t *space, uintptr_t start, uintptr_t end) {
	int interrupt_save;
	rwlock_acquire_write(&space->lock, &interrupt_save);
	int ret = vmm_space_raw_unmap_range(space, start, end);
	rwlock_release_write(&space->lock, &interrupt_save);
	return ret;
}

void vmm_space_unmap_all(vmm_space_t *space) {
	int interrupt_save;
	rwlock_acquire_write(&space->lock, &interrupt_save);
	vmm_seg_t *current = container_of(space->segs.first_node, vmm_seg_t, node);
	while (current) {
		vmm_seg_t *next = container_of(current->node.next, vmm_seg_t, node);
		vmm_space_raw_unmap(space, current);
		current = next;
	}
	rwlock_release_write(&space->lock, &interrupt_save);
}

int vmm_space_sync(vmm_space_t *space, vmm_seg_t *seg, uintptr_t start, uintptr_t end, int flags) {
	// TODO : pass the adddress space to the driver
	(void)space;
	spinlock_acquire(&seg->lock);
	if (!seg->ops || !seg->ops->msync) {
		spinlock_release(&seg->lock);
		return 0;
	}
	// cap start/end
	if (start < seg->start) start = seg->start;
	if (end > seg->end) end = seg->end;
	int ret = seg->ops->msync(seg, start, end, flags);
	spinlock_release(&seg->lock);
	return ret;
}

static int vmm_space_raw_sync_range(vmm_space_t *space, uintptr_t start, uintptr_t end, int flags) {
	foreach (node, &space->segs) {
		vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
		if (seg->end <= start) continue;
		if (seg->start >= end) break;
		int ret = vmm_space_sync(space, seg, start, end, flags);
		if (ret < 0) return ret;
	}

	return 0;
}

int vmm_space_sync_range(vmm_space_t *space, uintptr_t start, uintptr_t end, int flags) {
	int interrupt_save;
	rwlock_acquire_read(&space->lock, &interrupt_save);
	int ret = vmm_space_raw_sync_range(space, start, end, flags);
	rwlock_release_read(&space->lock, &interrupt_save);
	return ret;
}

static void vmm_clone_seg(vmm_space_t *parent, vmm_space_t *child, vmm_seg_t *seg) {
	spinlock_acquire(&seg->lock);
	vmm_seg_t *new_seg    = slab_alloc(&vmm_seg_slab);
	new_seg->prot         = seg->prot;
	new_seg->flags        = seg->flags;
	new_seg->start        = seg->start;
	new_seg->end          = seg->end;
	new_seg->ops          = seg->ops;
	new_seg->private_data = seg->private_data;
	new_seg->fd           = vfs_dup(seg->fd);

	long prot = seg->prot;
	if (seg->flags & VMM_FLAG_PRIVATE) {
		prot &= ~MMU_FLAG_WRITE;
	}
	// remap in child
	for (uintptr_t addr = seg->start; addr < seg->end; addr += PAGE_SIZE) {
		uintptr_t phys = mmu_space_virt2phys(parent->addrspace, (void *)addr);
		// do not touch ref count of IO mapping
		// because they actually do not have a ref count
		if (!(seg->flags & VMM_FLAG_IO)) {
			pmm_retain(phys);
		}
		mmu_map_page(child->addrspace, phys, addr, prot);
	}

	if (seg->flags & VMM_FLAG_PRIVATE) {
		// we need to remap as readonly in the parent too
		for (uintptr_t addr = seg->start; addr < seg->end; addr += PAGE_SIZE) {
			mmu_set_flags(parent->addrspace, addr, prot);
		}
	}

	vmm_seg_t *prev = NULL;
	foreach (node, &child->segs) {
		vmm_seg_t *current = container_of(node, vmm_seg_t, node);
		if (current->start > seg->end) {
			break;
		}
		prev = current;
	}
	list_add_after(&child->segs, prev ? &prev->node : NULL, &new_seg->node);

	if (seg->ops && seg->ops->open) {
		seg->ops->open(new_seg);
	}
	spinlock_release(&seg->lock);
}

int vmm_clone(vmm_space_t *parent, vmm_space_t *child) {
	child->total_size   = parent->total_size;
	child->peak_size    = parent->peak_size;
	child->private_size = parent->private_size;
	child->shared_size  = parent->shared_size;
	child->file_size    = parent->file_size;
	child->anon_size    = parent->anon_size;
	int interrupt_save;
	rwlock_acquire_read(&parent->lock, &interrupt_save);
	foreach (node, &parent->segs) {
		vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
		vmm_clone_seg(parent, child, seg);
	}
	rwlock_release_read(&parent->lock, &interrupt_save);
	return 0;
}
