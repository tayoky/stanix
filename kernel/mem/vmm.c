#include <kernel/scheduler.h>
#include <kernel/vmm.h>
#include <kernel/mmu.h>
#include <kernel/string.h>
#include <kernel/kernel.h>
#include <kernel/list.h>
#include <kernel/print.h>
#include <kernel/signal.h>
#include <kernel/pmm.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>


static void vmm_unmap_all_space(vmm_space_t *space);

static int vmm_handle_fault(vmm_seg_t *seg, uintptr_t addr, int prot) {
	spinlock_acquire(&seg->lock);
	uintptr_t vpage = PAGE_ALIGN_DOWN((uintptr_t)addr);
	uintptr_t phys  = mmu_virt2phys((void *)vpage);

	if (seg->ops && seg->ops->fault) {
		if (seg->ops->fault(seg, addr, prot)) {
			spinlock_release(&seg->lock);
			return 1;
		}
	}

	if ((seg->flags & VMM_FLAG_PRIVATE) && (seg->prot & MMU_FLAG_WRITE) && prot == MMU_FLAG_WRITE) {
		// we failed a write but we have write perm
		// it's CoW
		if (atomic_load(&pmm_page_info(phys)->ref_count) <= 1) {
			// other processes already copied
			mmu_set_flags(get_current_proc()->vmm_space.addrspace, vpage, seg->prot);
		} else {
			// we need to copy
			uintptr_t new_page = pmm_dup_page(phys);
			if (new_page == PAGE_INVALID) {
				// not looking good
				send_sig_task(get_current_task(), SIGBUS);
				spinlock_release(&seg->lock);
				return 1;
			}
			pmm_free_page(phys);
			mmu_map_page(get_current_proc()->vmm_space.addrspace, new_page, vpage, seg->prot);
		}
		spinlock_release(&seg->lock);
		return 1;
	}
	spinlock_release(&seg->lock);
	return 0;
}

int vmm_fault_report(uintptr_t addr, int prot) {
	int interrupt_save;
	rwlock_acquire_read(&get_current_proc()->vmm_space.lock, &interrupt_save);
	foreach(node, &get_current_proc()->vmm_space.segs) {
		vmm_seg_t *seg = (vmm_seg_t *)node;
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
	vmm_unmap_all_space(space);
	mmu_delete_addr_space(space->addrspace);
	destroy_list(&space->segs);
}

static vmm_seg_t *vmm_create_seg(vmm_space_t *vmm_space, uintptr_t address, size_t size, long prot, int flags) {
	vmm_seg_t *prev = NULL;
	if (address) {
		// we need to page align everything
		uintptr_t end = PAGE_ALIGN_UP(address + size);
		address = PAGE_ALIGN_DOWN(address);
		foreach(node, &vmm_space->segs) {
			vmm_seg_t *current = (vmm_seg_t *)node;
			if (current->start < end && current->end > address) {
				// there is aready a seg here
				return NULL;
			}
			if (current->start > end) {
				break;
			}
			prev = current;
		}
	} else {
		// we need to page align everything
		size = PAGE_ALIGN_UP(size);

		// no address ? we need to find one ourself
		foreach(node, &vmm_space->segs) {
			vmm_seg_t *current = (vmm_seg_t *)node;
			vmm_seg_t *next = (vmm_seg_t *)node->next;
			if (!next || next->start - current->end >= size) {
				address = current->end;
				prev = current;
				break;
			}
		}

		if (!address) {
			// hardcoded userspace start
			address = PAGE_SIZE * 12;
			prev = NULL;
		}
	}

	vmm_seg_t *new_seg = kmalloc(sizeof(vmm_seg_t));
	memset(new_seg, 0, sizeof(vmm_seg_t));
	new_seg->start = address;
	new_seg->end   = address + size;
	new_seg->prot  = prot;
	new_seg->flags = flags;

	list_add_after(&vmm_space->segs, prev ? &prev->node : NULL, &new_seg->node);

	return new_seg;
}

int vmm_map(uintptr_t address, size_t size, long prot, int flags, vfs_fd_t *fd, off_t offset, vmm_seg_t **seg) {
	// we need size to be aligned
	size = PAGE_ALIGN_UP(size);

	vmm_seg_t *new_seg = vmm_create_seg(&get_current_proc()->vmm_space, address, size, prot, flags);
	if (!new_seg) return -EEXIST;

	//kdebugf("map %p size : %lx\n", new_seg->start, size);
	int ret = 0;
	if (flags & VMM_FLAG_ANONYMOUS) {
		fd = NULL;
		for (uintptr_t addr=new_seg->start; addr < new_seg->end; addr += PAGE_SIZE) {
			// TODO : handle error from pmm_allocate_page
			uintptr_t page;
			if (flags & VMM_FLAG_SHARED) {
				page = pmm_allocate_page();
				memset((void *)(kernel->hhdm + page), 0, PAGE_SIZE);
				mmu_map_page(get_current_proc()->vmm_space.addrspace, page, addr, prot);
			} else {
				page = pmm_get_zero_page();
				mmu_map_page(get_current_proc()->vmm_space.addrspace, page, addr, prot & ~MMU_FLAG_WRITE);
			}
		}
	} else if (fd) {
		ret = vfs_mmap(fd, offset, new_seg);
	} else {
		ret = -EINVAL;
	}

	if (ret < 0) {
		list_remove(&get_current_proc()->vmm_space.segs, &new_seg->node);
		kfree(new_seg);
	} else {
		if (seg) *seg = new_seg;
		if (fd) {
			new_seg->fd     = vfs_dup(fd);
			new_seg->offset = offset;
		}
	}
	return ret;
}

int vmm_chprot(vmm_seg_t *seg, long prot) {
	spinlock_acquire(&seg->lock);
	if (seg->ops && seg->ops->can_mprotect) {
		int ret = seg->ops->can_mprotect(seg, prot);
		if (ret < 0) {
			spinlock_release(&seg->lock);
			return ret;
		}
	}
	seg->prot = prot;

	for (uintptr_t addr=seg->start; addr < seg->end; addr += PAGE_SIZE) {
		uintptr_t phys = mmu_space_virt2phys(get_current_proc()->vmm_space.addrspace, (void *)addr);
		if ((seg->flags & VMM_FLAG_PRIVATE) && pmm_page_info(phys)->ref_count > 1) {
			// we are doing CoW
			mmu_set_flags(get_current_proc()->vmm_space.addrspace, addr, prot & ~MMU_FLAG_WRITE);
		} else {
			mmu_set_flags(get_current_proc()->vmm_space.addrspace, addr, prot);
		}
	}
	spinlock_release(&seg->lock);
	return 0;
}

static void vmm_raw_unmap(vmm_seg_t *seg) {
	spinlock_acquire(&seg->lock);
	list_remove(&get_current_proc()->vmm_space.segs, &seg->node);

	//kdebugf("unmap %p to %p\n", seg->start, seg->end);

	if (seg->ops && seg->ops->close) {
		seg->ops->close(seg);
	}
	vfs_close(seg->fd);

	// IO cannot be allocated/freed using the PMM
	// so do not free them
	// it's the driver job to do it
	if (!(seg->flags & VMM_FLAG_IO)) {
		for (uintptr_t addr=seg->start; addr < seg->end; addr += PAGE_SIZE) {
			uintptr_t page = mmu_virt2phys((void *)addr);
			if (page == PAGE_INVALID) continue;
			pmm_free_page(page);
		}
	}

	for (uintptr_t addr=seg->start; addr < seg->end; addr += PAGE_SIZE) {
		mmu_unmap_page(get_current_proc()->vmm_space.addrspace, addr);
	}

	kfree(seg);
}


void vmm_unmap(vmm_seg_t *seg) {
	int interrupt_save;
	rwlock_acquire_write(&get_current_proc()->vmm_space.lock, &interrupt_save);
	vmm_raw_unmap(seg);
	rwlock_release_write(&get_current_proc()->vmm_space.lock, &interrupt_save);
}

int vmm_unmap_range(uintptr_t start, uintptr_t end) {
	int interrupt_save;
	rwlock_acquire_write(&get_current_proc()->vmm_space.lock, &interrupt_save);
	vmm_seg_t *seg = (vmm_seg_t *)get_current_proc()->vmm_space.segs.first_node;
	for (vmm_seg_t *next; seg; seg = next) {
		next = (vmm_seg_t *)seg->node.next;

		if (seg->end <= start) continue;
		if (seg->start >= end) break;
		if (start > seg->start) {
			int ret = vmm_split(seg, start, NULL);
			if (ret > 0) return ret;

			// on the next iteration we will land on the newly created segment
			continue;
		}
		if (end < seg->end) {
			vmm_split(seg, end, NULL);
		}
		vmm_raw_unmap(seg);
	}
	rwlock_release_write(&get_current_proc()->vmm_space.lock, &interrupt_save);
	return 0;
}

static void vmm_unmap_all_space(vmm_space_t *space) {
	vmm_seg_t *current = (vmm_seg_t*)space->segs.first_node;
	while (current) {
		vmm_seg_t *next = (vmm_seg_t*)current->node.next;
		vmm_raw_unmap(current);
		current = next;
	}
}

void vmm_unmap_all(void) {
	int interrupt_save;
	rwlock_acquire_write(&get_current_proc()->vmm_space.lock, &interrupt_save);
	vmm_unmap_all_space(&get_current_proc()->vmm_space);
	rwlock_release_write(&get_current_proc()->vmm_space.lock, &interrupt_save);
}

int vmm_sync(vmm_seg_t *seg, uintptr_t start, uintptr_t end, int flags) {
	if (!seg->ops || !seg->ops->msync) {
		return 0;
	}
	// cap start/end
	if (start < seg->start) start = seg->start;
	if (end > seg->end) end   = seg->end;
	return seg->ops->msync(seg, start, end, flags);
}

int vmm_split(vmm_seg_t *seg, uintptr_t cut, vmm_seg_t **out_seg) {
	if (cut <= seg->start || cut >= seg->end) {
		return -EINVAL;
	}
	if (seg->ops && seg->ops->can_split) {
		int ret = seg->ops->can_split(seg, cut);
		if (ret < 0) return ret;
	}

	vmm_seg_t *new_seg = kmalloc(sizeof(vmm_seg_t));
	memset(new_seg, 0, sizeof(vmm_seg_t));
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

	list_add_after(&get_current_proc()->vmm_space.segs, &seg->node, &new_seg->node);

	if (out_seg) *out_seg = new_seg;
	return 0;
}

static void vmm_clone_seg(vmm_space_t *parent, vmm_space_t *child, vmm_seg_t *seg) {
	vmm_seg_t *new_seg = kmalloc(sizeof(vmm_seg_t));
	memset(new_seg, 0, sizeof(vmm_seg_t));
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
	for (uintptr_t addr=seg->start; addr < seg->end; addr += PAGE_SIZE) {
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
		for (uintptr_t addr=seg->start; addr < seg->end; addr += PAGE_SIZE) {
			mmu_set_flags(parent->addrspace, addr, prot);
		}
	}

	vmm_seg_t *prev = NULL;
	foreach(node, &child->segs) {
		vmm_seg_t *current = (vmm_seg_t *)node;
		if (current->start > seg->end) {
			break;
		}
		prev = current;
	}
	list_add_after(&child->segs, prev ? &prev->node : NULL, &new_seg->node);

	if (seg->ops && seg->ops->open) {
		seg->ops->open(new_seg);
	}
}


int vmm_clone(vmm_space_t *parent, vmm_space_t *child) {
	foreach (node, &parent->segs) {
		vmm_seg_t *seg = (vmm_seg_t *)node;
		vmm_clone_seg(parent, child, seg);
	}
	return 0;
}
