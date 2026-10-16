#include <kernel/kernel.h>
#include <kernel/list.h>
#include <kernel/mmu.h>
#include <kernel/pmm.h>
#include <kernel/print.h>
#include <kernel/process.h>
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

static int vmm_cow(vmm_seg_t *seg, uintptr_t vpage, uintptr_t addr) {
	mmu_entry_t *entry = mmu_get_entry_and_acquire_lock(get_current_proc()->vmm_space.addrspace, vpage);
	if (!entry) return 0;
	uintptr_t phys = mmu_virt2phys((void *)vpage);
	if (atomic_load(&pmm_page_info(phys)->ref_count) <= 1) {
		// other processes already copied
		mmu_entry_or_flags(get_current_proc()->vmm_space.addrspace, entry, MMU_FLAG_WRITE);
	} else {
		// we need to copy
		uintptr_t new_page = pmm_dup_page(phys);
		if (new_page == PAGE_INVALID) {
			mmu_entry_release_lock(entry);
			// not looking good
			siginfo_t siginfo = {
				.si_signo = SIGBUS,
				.si_addr  = (void*)addr,
			};
			if (signal_send_siginfo_task(get_current_task(), &siginfo) < 0) {
				// retry with a lightweight signal
				// we lose some information (like address) but at least the process get killed
				signal_send_siginfo_lightweight_task(get_current_task(), SIGBUS);
			}
			return -ENOMEM;
		}
		pmm_release_page(phys);
		mmu_entry_map(get_current_proc()->vmm_space.addrspace, entry, new_page, seg->prot);
	}
	mmu_entry_release_lock(entry);
	return 0;
}

static int vmm_handle_fault(vmm_seg_t *seg, uintptr_t addr, int prot) {
	mutex_acquire(&seg->lock);
	uintptr_t vpage = PAGE_ALIGN_DOWN((uintptr_t)addr);

	if (seg->ops && seg->ops->fault) {
		if (seg->ops->fault(seg, addr, prot)) {
			mutex_release(&seg->lock);
			return 1;
		}
	}

	if ((seg->flags & VMM_FLAG_PRIVATE) && (seg->prot & MMU_FLAG_WRITE) && prot == MMU_FLAG_WRITE) {
		// we failed a write but we have write perm
		// it's CoW
		int ret = vmm_cow(seg, vpage, addr);
		if (ret > 0 && vpage + PAGE_SIZE < seg->end) {
			vmm_cow(seg, vpage + PAGE_SIZE, addr);
		}
		mutex_release(&seg->lock);
		return 1;
	}
	mutex_release(&seg->lock);
	return 0;
}

int vmm_fault_report(uintptr_t addr, int prot) {
	if (!get_current_proc()) return 0;
	rwsem_acquire_read(&get_current_proc()->vmm_space.lock);
	atomic_fetch_add(&get_current_proc()->vmm_space.page_faults, 1);
	foreach (node, &get_current_proc()->vmm_space.segs) {
		vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
		if (seg->start > addr) break;
		if (seg->end > addr) {
			// we found a seg to report to
			int ret = vmm_handle_fault(seg, addr, prot);
			rwsem_release_read(&get_current_proc()->vmm_space.lock);
			return ret;
		}
	}
	rwsem_release_read(&get_current_proc()->vmm_space.lock);
	return 0;
}

void vmm_init_space(vmm_space_t *space) {
	memset(space, 0, sizeof(vmm_space_t));
	space->addrspace = mmu_space_init();
}

void vmm_destroy_space(vmm_space_t *space) {
	vmm_space_unmap_all(space);
	mmu_space_destroy(space->addrspace);
	list_destroy(&space->segs);
}

vmm_space_t *vmm_get_current_space(void) {
	return &get_current_proc()->vmm_space;
}

int vmm_space_split(vmm_space_t *space, vmm_seg_t *seg, uintptr_t cut, vmm_seg_t **out_seg) {
	mutex_acquire(&seg->lock);
	if (cut <= seg->start || cut >= seg->end) {
		mutex_release(&seg->lock);
		return -EINVAL;
	}
	if (seg->ops && seg->ops->can_split) {
		int ret = seg->ops->can_split(seg, cut);
		if (ret < 0) {
			mutex_release(&seg->lock);
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

	if (seg->ops && seg->ops->dup) {
		seg->ops->dup(new_seg);
	}

	list_add_after(&space->segs, &seg->node, &new_seg->node);

	if (out_seg) *out_seg = new_seg;
	mutex_release(&seg->lock);
	return 0;
}

static vmm_seg_t *vmm_space_raw_create_seg(vmm_space_t *space, uintptr_t address, size_t size, long prot, int flags) {
	vmm_seg_t *prev = NULL;
	if (address) {
		uintptr_t end = address + size;

		// first remove any overlaping mapping
		int ret = vmm_space_raw_unmap_range(space, address, end);
		if (ret < 0) return ERR2PTR(ret);

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
	mutex_acquire(&new_seg->lock);

	list_add_after(&space->segs, prev ? &prev->node : NULL, &new_seg->node);

	return new_seg;
}

static vmm_seg_t *vmm_space_raw_map(vmm_space_t *space, uintptr_t address, size_t size, long prot, int flags, vfs_fd_t *fd, off_t offset) {
	// we need size to be aligned
	size = PAGE_ALIGN_UP(size);

	vmm_seg_t *new_seg = vmm_space_raw_create_seg(space, address, size, prot, flags);
	if (IS_ERR(new_seg)) return new_seg;

	// kdebugf("map %p size : %lx\n", new_seg->start, size);
	int ret = 0;
	if (flags & VMM_FLAG_ANONYMOUS) {
		fd = NULL;
		mmu_entry_t *entry = NULL;
		for (uintptr_t addr = new_seg->start; addr < new_seg->end; addr += PAGE_SIZE) {
			if (flags & VMM_FLAG_SHARED) {
				uintptr_t page;
				page = pmm_allocate_page();
				if (page == PAGE_INVALID) {
					// unmap already mapped pages
unmap_pages:
					mmu_unmap_and_release_pages(space->addrspace, new_seg->start, addr - new_seg->start);
					ret = -ENOMEM;
					goto error;
				}
				memset(mmu_phys2virt(page), 0, PAGE_SIZE);
				// calling mmu_map_pages in a loop is slow as hell
				// might come back later on this
				if (mmu_map_pages(space->addrspace, addr, page, PAGE_SIZE, prot) < 0) {
					pmm_release_page(page);
					goto unmap_pages;
				}
			} else {
				uintptr_t page = pmm_get_zero_page();
				entry = mmu_create_next_entry_and_acquire_lock(space->addrspace, addr, entry, addr - PAGE_SIZE);
				if (!entry) goto unmap_pages;
				pmm_retain(page);
				mmu_entry_map(space->addrspace, entry, page, prot & ~MMU_FLAG_WRITE);
			}
		}
		if (entry) mmu_entry_release_lock(entry);
	} else if (fd) {
		ret = vfs_mmap(fd, offset, new_seg);
	} else {
		ret = -EINVAL;
	}

	if (ret < 0) {
error:
		list_remove(&space->segs, &new_seg->node);
		mutex_release(&new_seg->lock);
		slab_free(new_seg);
		return ERR2PTR(ret);
	}

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
	mutex_release(&new_seg->lock);
	return new_seg;
}

vmm_seg_t *vmm_space_map(vmm_space_t *space, uintptr_t address, size_t size, long prot, int flags, struct vfs_fd *fd, off_t offset) {
	rwsem_acquire_write(&space->lock);
	vmm_seg_t *new_seg = vmm_space_raw_map(space, address, size, prot, flags, fd, offset);
	rwsem_release_write(&space->lock);
	return new_seg;
}

int vmm_space_chprot(vmm_space_t *space, vmm_seg_t *seg, long prot) {
	mutex_acquire(&seg->lock);
	if (seg->ops && seg->ops->can_mprotect) {
		int ret = seg->ops->can_mprotect(seg, prot);
		if (ret < 0) {
			mutex_release(&seg->lock);
			return ret;
		}
	}
	seg->prot = prot;

	rcu_acquire_read();
	mmu_foreach_entry_range (vaddr, entry, space->addrspace, seg->start, seg->end) {
		uintptr_t phys = mmu_entry_get_page(space->addrspace, entry);
		if ((seg->flags & VMM_FLAG_PRIVATE) && pmm_page_info(phys)->ref_count > 1) {
			// we are doing CoW
			mmu_entry_set_flags(space->addrspace, entry, prot & ~MMU_FLAG_WRITE);
		} else {
			mmu_entry_set_flags(space->addrspace, entry, prot);
		}
	}
	rcu_release_read();
	mutex_release(&seg->lock);
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
	// FIXME : we might need to acquire write lock since chrprot_range can split
	rwsem_acquire_read(&space->lock);
	int ret = vmm_space_raw_chprot_range(space, start, end, prot);
	rwsem_release_read(&space->lock);
	return ret;
}

static void vmm_space_raw_unmap(vmm_space_t *space, vmm_seg_t *seg) {
	mutex_acquire(&seg->lock);
	list_remove(&space->segs, &seg->node);
	mutex_release(&seg->lock);
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
		seg->ops->msync(seg, seg->start, seg->end, VMM_FLAG_ASYNC);
	}

	if (seg->ops && seg->ops->destroy) {
		seg->ops->destroy(seg);
	}
	vfs_close(seg->fd);

	// IO cannot be allocated/freed using the pmm
	// so do not free them
	// it's the driver job to do it
	if (seg->flags & VMM_FLAG_IO) {
		mmu_unmap_pages(space->addrspace, seg->start, VMM_SIZE(seg));
	} else {
		mmu_unmap_and_release_pages(space->addrspace, seg->start, VMM_SIZE(seg));
	}
	slab_free(seg);
}

void vmm_space_unmap(vmm_space_t *space, vmm_seg_t *seg) {
	rwsem_acquire_write(&space->lock);
	vmm_space_raw_unmap(space, seg);
	rwsem_release_write(&space->lock);
}

static int vmm_space_raw_unmap_range(vmm_space_t *space, uintptr_t start, uintptr_t end) {
	if (list_is_empty(&space->segs)) {
		return 0;
	}

	vmm_seg_t *seg = container_of(space->segs.first_node, vmm_seg_t, node);
	for (vmm_seg_t *next; seg; seg = next) {
		next = container_of(seg->node.next, vmm_seg_t, node);

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
	rwsem_acquire_write(&space->lock);
	int ret = vmm_space_raw_unmap_range(space, start, end);
	rwsem_release_write(&space->lock);
	return ret;
}

void vmm_space_unmap_all(vmm_space_t *space) {
	rwsem_acquire_write(&space->lock);
	vmm_seg_t *current = container_of(space->segs.first_node, vmm_seg_t, node);
	while (current) {
		vmm_seg_t *next = container_of(current->node.next, vmm_seg_t, node);
		vmm_space_raw_unmap(space, current);
		current = next;
	}
	rwsem_release_write(&space->lock);
}

int vmm_space_sync(vmm_space_t *space, vmm_seg_t *seg, uintptr_t start, uintptr_t end, int flags) {
	// TODO : pass the adddress space to the driver
	(void)space;
	mutex_acquire(&seg->lock);
	if (!seg->ops || !seg->ops->msync) {
		mutex_release(&seg->lock);
		return 0;
	}
	// cap start/end
	if (start < seg->start) start = seg->start;
	if (end > seg->end) end = seg->end;
	int ret = seg->ops->msync(seg, start, end, flags);
	mutex_release(&seg->lock);
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
	rwsem_acquire_read(&space->lock);
	int ret = vmm_space_raw_sync_range(space, start, end, flags);
	rwsem_release_read(&space->lock);
	return ret;
}

static int vmm_clone_seg(vmm_space_t *parent, vmm_space_t *child, vmm_seg_t *seg) {
	vmm_seg_t *new_seg = slab_alloc(&vmm_seg_slab);
	if (!new_seg) return -ENOMEM;
	mutex_acquire(&seg->lock);
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
	rcu_acquire_read();
	mmu_entry_t *child_entry = NULL;
	uintptr_t old_vaddr = 0;
	mmu_foreach_entry_range (vaddr, parent_entry, parent->addrspace, seg->start, seg->end) {
		uintptr_t page = mmu_entry_get_page(parent->addrspace, parent_entry);
		// do not touch ref count of IO mapping
		// because they actually do not have a ref count
		if (!(seg->flags & VMM_FLAG_IO)) {
			pmm_retain(page);
		}
		rcu_release_read();

		child_entry = mmu_create_next_entry_and_acquire_lock(child->addrspace, vaddr, child_entry, old_vaddr);
		old_vaddr = vaddr;
		if (!child_entry) {
			kassert("TODO : handle memory failure");
		}
		mmu_entry_map(child->addrspace, child_entry, page, prot);
		rcu_acquire_read();
	}
	rcu_release_read();
	if (child_entry) mmu_entry_release_lock(child_entry);

	if (seg->flags & VMM_FLAG_PRIVATE) {
		// we need to remap as readonly in the parent too
		rcu_acquire_read();
		mmu_foreach_entry_range (vaddr, entry, parent->addrspace, seg->start, seg->end) {
			mmu_entry_get_and_clear_flags(parent->addrspace, entry, MMU_FLAG_WRITE);
		}
		rcu_release_read();
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

	if (seg->ops && seg->ops->dup) {
		seg->ops->dup(new_seg);
	}
	mutex_release(&seg->lock);
	return 0;
}

int vmm_clone(vmm_space_t *parent, vmm_space_t *child) {
	child->total_size   = parent->total_size;
	child->peak_size    = parent->peak_size;
	child->private_size = parent->private_size;
	child->shared_size  = parent->shared_size;
	child->file_size    = parent->file_size;
	child->anon_size    = parent->anon_size;
	rwsem_acquire_read(&parent->lock);
	foreach (node, &parent->segs) {
		vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
		vmm_clone_seg(parent, child, seg);
	}
	rwsem_release_read(&parent->lock);
	return 0;
}
