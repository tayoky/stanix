#include <kernel/kernel.h>
#include <kernel/avltree.h>
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

static void vmm_send_sigbus(uintptr_t addr) {
	siginfo_t siginfo = {
		.si_signo = SIGBUS,
		.si_addr  = (void*)addr,
	};
	if (signal_send_siginfo_task(get_current_task(), &siginfo) < 0) {
		// retry with a lightweight signal
		// we lose some information (like address) but at least the process get killed
		signal_send_siginfo_lightweight_task(get_current_task(), SIGBUS);
	}
}


static int vmm_map_page(vmm_seg_t *seg, uintptr_t vpage, long prot) {
	if (!seg->ops || !seg->ops->get_page) return 0;
	if (mmu_virt2phys((void*)vpage) != PAGE_INVALID) {
		// the page is already mapped
		return VMM_FAULT_NOT_HANDLED;
	}

	uintptr_t page = seg->ops->get_page(seg, vpage);
	if (page == PAGE_INVALID) {
		return VMM_FAULT_FAILED;
	}

	long mapping_prot = seg->prot;
	if (!(seg->flags & VMM_FLAG_IO) && (seg->flags & VMM_FLAG_PRIVATE)) {
		// we need to setup CoW
		if (prot & MMU_FLAG_WRITE) {
			// we need to do the copy right now
			uintptr_t new_page = pmm_dup_page(page);
			if (!(seg->flags & VMM_FLAG_IO)) {
				pmm_release_page(page);
			}
			if (new_page == PAGE_INVALID) {
				return VMM_FAULT_FAILED;
			}
			page = new_page;
		} else {
			mapping_prot &= ~MMU_FLAG_WRITE;
		}
	}

	mmu_entry_t *entry = mmu_create_entry_and_acquire_lock(get_current_proc()->vmm_space.addrspace, vpage);
	if (!entry) {
		if (!(seg->flags & VMM_FLAG_IO)) {
			pmm_release_page(page);
		}
		return VMM_FAULT_FAILED;
	}

	if (mmu_entry_get_flags(get_current_proc()->vmm_space.addrspace, entry) & MMU_FLAG_PRESENT) {
		// the page was mapped in between, we raced
		mmu_entry_release_lock(entry);
		if (!(seg->flags & VMM_FLAG_IO)) {
			pmm_release_page(page);
		}
		return VMM_FAULT_SUCCED;
	}

	mmu_entry_map(get_current_proc()->vmm_space.addrspace, entry, page, mapping_prot);
	mmu_entry_release_lock(entry);
	return VMM_FAULT_SUCCED;
}

static int vmm_cow(vmm_seg_t *seg, uintptr_t vpage) {
	mmu_entry_t *entry = mmu_get_entry_and_acquire_lock(get_current_proc()->vmm_space.addrspace, vpage);
	if (!entry) return VMM_FAULT_NOT_HANDLED;
	uintptr_t page = mmu_entry_get_page(get_current_proc()->vmm_space.addrspace, entry);
	if (page == PAGE_INVALID) return VMM_FAULT_NOT_HANDLED;
	if (atomic_load(&pmm_page_info(page)->ref_count) <= 1) {
		// other processes already copied
		mmu_entry_or_flags(get_current_proc()->vmm_space.addrspace, entry, MMU_FLAG_WRITE);
	} else {
		// we need to copy
		uintptr_t new_page = pmm_dup_page(page);
		if (new_page == PAGE_INVALID) {
			mmu_entry_release_lock(entry);
			// not looking good
			return VMM_FAULT_FAILED;
		}
		if (!(seg->flags & VMM_FLAG_IO)) {
			pmm_release_page(page);
		}
		mmu_entry_map_and_or_flags(get_current_proc()->vmm_space.addrspace, entry, new_page, MMU_FLAG_WRITE);
	}
	mmu_invalidate(vpage, PAGE_SIZE);
	mmu_entry_release_lock(entry);
	return VMM_FAULT_SUCCED;
}

static int vmm_handle_fault(vmm_seg_t *seg, uintptr_t addr, int prot) {
	mutex_acquire(&seg->lock);
	uintptr_t vpage = PAGE_ALIGN_DOWN(addr);
	kdebugf("handle fault at %p\n", vpage);

	if (seg->ops && seg->ops->fault) {
		switch (seg->ops->fault(seg, addr, prot)) {
		case VMM_FAULT_FAILED:
			vmm_send_sigbus(addr);
			// fallthrough
		case VMM_FAULT_SUCCED:
			mutex_release(&seg->lock);
			return 1;
		}
	}

	if (!(seg->prot & prot)) {
		// violation of permissions
		mutex_release(&seg->lock);
		return 0;
	}

	// calculate the size of the fault, with prediction
	size_t fault_size;
	if (vpage > seg->last_fault_page && vpage <= seg->last_fault_page + seg->last_fault_size) {
		// the fault is continuous to the previous one
		// continue predicition
		fault_size = seg->last_fault_size;
		seg->last_fault_counter++;
	} else {
		// the fault isen't continuous to the previous one
		// reset predicition
		fault_size = VMM_DEFAULT_FAULT_SIZE;
		seg->last_fault_counter = 0;
	}

	if (seg->last_fault_counter * PAGE_SIZE >= fault_size) {
		// we pass to the next stage
		seg->last_fault_counter = 0;
		fault_size *= 2;
		if (fault_size > VMM_MAX_FAULT_SIZE) {
			fault_size = VMM_MAX_FAULT_SIZE;
		}
	}
	seg->last_fault_size = fault_size;
	seg->last_fault_page = vpage;

	if (seg->ops && seg->ops->preload) {
		// we ignore errors
		// because a failed preload is not fatal
		// and we also preload a few mores pages beyond the size of the fault
		// so they are ready for the next fault
		seg->ops->preload(seg, vpage, fault_size * 2 - VMM_DEFAULT_FAULT_SIZE);
	}

	for (uintptr_t current = vpage; current < vpage + fault_size && current < seg->end; current += PAGE_SIZE) {
		int ret = vmm_map_page(seg, current, prot);
		if (ret == VMM_FAULT_NOT_HANDLED && (seg->flags & VMM_FLAG_PRIVATE) && prot == MMU_FLAG_WRITE) {
			// we failed a write but we have write perm
			// it's CoW
			ret = vmm_cow(seg, current);
		}

		switch (ret) {
		case VMM_FAULT_NOT_HANDLED:
			if (current == vpage) {
				mutex_release(&seg->lock);
				return 0;
			}
			break;
		case VMM_FAULT_FAILED:
			// note that an ahead failed fault (prediction) is non fatal
			// because we actually do not need the page yet
			if (current == vpage) {
				// not looking good
				vmm_send_sigbus(addr);
			}
			mutex_release(&seg->lock);
			return 1;
		}
	}
	mutex_release(&seg->lock);
	return 1;
}

int vmm_fault_report(uintptr_t addr, int prot) {
	if (!get_current_proc()) return 0;
	rwsem_acquire_read(&get_current_proc()->vmm_space.lock);
	atomic_fetch_add(&get_current_proc()->vmm_space.page_faults, 1);
	vmm_seg_t *seg = vmm_space_lookup(vmm_get_current_space(), addr);
	int ret = 0;
	if (seg) {
		// we found a seg to report to
		ret = vmm_handle_fault(seg, addr, prot);
	}
	rwsem_release_read(&get_current_proc()->vmm_space.lock);
	return ret;
}

static int vmm_tree_compare_nodes(avltree_t *avltree, avltree_node_t *a, avltree_node_t *b) {
	(void)avltree;
	vmm_seg_t *seg_a = container_of(a, vmm_seg_t, node);
	vmm_seg_t *seg_b = container_of(b, vmm_seg_t, node);

	if (seg_a->end <= seg_b->start) {
		return -1;
	} else if (seg_a->start >= seg_b->end) {
		return 1;
	} else {
		return 0;
	}
}

static int vmm_tree_compare_key(avltree_t *avltree, void *key, avltree_node_t *node) {
	(void)avltree;
	uintptr_t addr = *(uintptr_t *)key;
	vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
	if (addr < seg->start) {
		return -1;
	} else if (addr >= seg->end) {
		return 1;
	} else {
		return 0;
	}
}

static void vmm_tree_recalculate(avltree_t *avltree, avltree_node_t *node) {
	(void)avltree;
	vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
	(void)seg;
	// TODO
}

static avltree_ops_t vmm_tree_ops = {
	.compare_nodes = vmm_tree_compare_nodes,
	.compare_key   = vmm_tree_compare_key,
	.recalculate   = vmm_tree_recalculate,
};

void vmm_init_space(vmm_space_t *space) {
	memset(space, 0, sizeof(vmm_space_t));
	space->addrspace = mmu_space_init();
	avltree_init(&space->segs, &vmm_tree_ops);
}

void vmm_destroy_space(vmm_space_t *space) {
	vmm_space_unmap_all(space);
	mmu_space_destroy(space->addrspace);
	avltree_destroy(&space->segs);
}

vmm_space_t *vmm_get_current_space(void) {
	return &get_current_proc()->vmm_space;
}

vmm_seg_t *vmm_space_lookup(vmm_space_t *space, uintptr_t addr) {
	avltree_node_t *node = avltree_lookup(&space->segs, &addr);
	if (!node) return NULL;
	return container_of(node, vmm_seg_t, node);
}

int vmm_space_split(vmm_space_t *space, vmm_seg_t *seg, uintptr_t cut, vmm_seg_t **out_seg) {
	rwsem_assert_write_acquired(&space->lock);
	kassert(cut % PAGE_SIZE == 0);
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
	if (!new_seg) return -ENOMEM;
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

	avltree_insert(&space->segs, &new_seg->node);

	if (out_seg) *out_seg = new_seg;
	mutex_release(&seg->lock);
	return 0;
}

static int vmm_has_perm(vfs_fd_t *fd, long prot, int flags) {
	if (flags & VMM_FLAG_ANONYMOUS) {
		// we can always do anything on anonymous mappings
		return 1;
	}

	if (fd->flags & O_WRONLY) {
		return 0;
	}
	if ((flags & VMM_FLAG_SHARED) && (prot & MMU_FLAG_WRITE) && !(fd->flags & O_RDWR)) {
		return 0;
	}
	return 1;
}

static vmm_seg_t *vmm_space_raw_create_seg(vmm_space_t *space, uintptr_t address, size_t size, long prot, int flags) {
	int ret = 0;
	vmm_seg_t *new_seg = slab_alloc(&vmm_seg_slab);
	if (!new_seg) return ERR2PTR(-ENOMEM);
	if (address) {
		uintptr_t end = address + size;

		// remove any overlaping mapping
		ret = vmm_space_raw_unmap_range(space, address, end);
		if (ret < 0) {
error:
			slab_free(new_seg);
			return ERR2PTR(ret);
		}
	} else {
		// no address ? we need to find one ourself

		// hardcoded userspace start
		address = PAGE_SIZE * 128;
		avltree_foreach (node, &space->segs) {
			vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
			if (seg->start - address >= size) {
				break;
			}
			address = seg->end;
		}
		if (!address) {
			ret = -ENOMEM;
			goto error;
		}
	}

	new_seg->start     = address;
	new_seg->end       = address + size;
	new_seg->prot      = prot;
	new_seg->flags     = flags;
	mutex_acquire(&new_seg->lock);

	avltree_insert(&space->segs, &new_seg->node);
	return new_seg;
}

static vmm_seg_t *vmm_space_raw_map(vmm_space_t *space, uintptr_t address, size_t size, long prot, int flags, vfs_fd_t *fd, off_t offset) {
	// we need size to be aligned
	size = PAGE_ALIGN_UP(size);
		
	if (!vmm_has_perm(fd, prot, flags)) {
		return ERR2PTR(-EACCES);
	}

	vmm_seg_t *new_seg = vmm_space_raw_create_seg(space, address, size, prot, flags);
	if (IS_ERR(new_seg)) return new_seg;

	//kdebugf("map %p size : %lx\n", new_seg->start, size);
	int ret = 0;
	if (flags & VMM_FLAG_ANONYMOUS) {
		fd = NULL;
		if (flags & VMM_FLAG_SHARED) {
			ret = mmu_allocate_zero_and_map_pages(space->addrspace, new_seg->start, new_seg->end - new_seg->start, prot);
			if (ret < 0) goto error;
		} else {
			uintptr_t page = pmm_get_zero_page();
			mmu_entry_t *entry = NULL;
			for (uintptr_t addr = new_seg->start; addr < new_seg->end; addr += PAGE_SIZE) {
				entry = mmu_create_next_entry_and_acquire_lock(space->addrspace, addr, entry, addr - PAGE_SIZE);
				if (!entry) {
					// unmap already mapped pages
					mmu_unmap_and_release_pages(space->addrspace, new_seg->start, addr - new_seg->start);
					if (space == &get_current_proc()->vmm_space) mmu_invalidate(new_seg->start, addr - new_seg->start);
					ret = -ENOMEM;
					goto error;
				}
				pmm_retain(page);
				mmu_entry_map(space->addrspace, entry, page, prot & ~MMU_FLAG_WRITE);
			}
			if (entry) mmu_entry_release_lock(entry);
		}
	} else if (fd) {
		ret = vfs_mmap(fd, offset, new_seg);
	} else {
		ret = -EINVAL;
	}

	if (ret < 0) {
error:
		avltree_remove(&space->segs, &new_seg->node);
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
	if (!vmm_has_perm(seg->fd, prot, seg->flags)) {
		mutex_release(&seg->lock);
		return -EACCES;
	}
	if (seg->ops && seg->ops->can_mprotect) {
		int ret = seg->ops->can_mprotect(seg, prot);
		if (ret < 0) {
			mutex_release(&seg->lock);
			return ret;
		}
	}
	if (seg->prot == prot) {
		mutex_release(&seg->lock);
		return 0;
	}
	seg->prot = prot;

	rcu_acquire_read();
	mmu_foreach_entry_range (vaddr, entry, space->addrspace, seg->start, seg->end) {
		uintptr_t page = mmu_entry_get_page(space->addrspace, entry);
		if (page == PAGE_INVALID) continue;
		if ((seg->flags & VMM_FLAG_PRIVATE) && pmm_page_info(page)->ref_count > 1) {
			// we are doing CoW
			mmu_entry_set_flags(space->addrspace, entry, prot & ~MMU_FLAG_WRITE);
		} else {
			mmu_entry_set_flags(space->addrspace, entry, prot);
		}
	}
	rcu_release_read();
	if (space == &get_current_proc()->vmm_space) mmu_invalidate(seg->start, VMM_SIZE(seg));
	mutex_release(&seg->lock);
	return 0;
}

static int vmm_space_raw_chprot_range(vmm_space_t *space, uintptr_t start, uintptr_t end, long prot) {
	kassert(start % PAGE_SIZE == 0);
	kassert(end % PAGE_SIZE == 0);
	for (avltree_node_t *node = avltree_first_superior_or_equal(&space->segs, &start); node; node = avltree_next(&space->segs, node)) {
		vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
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
	rwsem_acquire_write(&space->lock);
	int ret = vmm_space_raw_chprot_range(space, start, end, prot);
	rwsem_release_write(&space->lock);
	return ret;
}

static void vmm_space_raw_unmap(vmm_space_t *space, vmm_seg_t *seg) {
	rwsem_assert_write_acquired(&space->lock);
	mutex_acquire(&seg->lock);
	avltree_remove(&space->segs, &seg->node);
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

	// flush shared mappings so we don't lose changes
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
		if (space == &get_current_proc()->vmm_space) mmu_invalidate(seg->start, VMM_SIZE(seg));
	} else {
		mmu_unmap_and_release_pages(space->addrspace, seg->start, VMM_SIZE(seg));
		if (space == &get_current_proc()->vmm_space) mmu_invalidate(seg->start, VMM_SIZE(seg));
	}
	slab_free(seg);
}

void vmm_space_unmap(vmm_space_t *space, vmm_seg_t *seg) {
	rwsem_acquire_write(&space->lock);
	vmm_space_raw_unmap(space, seg);
	rwsem_release_write(&space->lock);
}

static int vmm_space_raw_unmap_range(vmm_space_t *space, uintptr_t start, uintptr_t end) {
	kassert(start % PAGE_SIZE == 0);
	kassert(end % PAGE_SIZE == 0);

	avltree_node_t *node = avltree_first_superior_or_equal(&space->segs, &start);
	while (node) {
		vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
		if (seg->start >= end) break;
		node = avltree_next(&space->segs, node);

		if (start > seg->start) {
			int ret = vmm_space_split(space, seg, start, &seg);
			if (ret < 0) return ret;
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
	avltree_node_t *node = avltree_first(&space->segs);
	while (node) {
		vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
		node = avltree_next(&space->segs, node);
		vmm_space_raw_unmap(space, seg);
	}
	rwsem_release_write(&space->lock);
}

int vmm_space_sync(vmm_space_t *space, vmm_seg_t *seg, uintptr_t start, uintptr_t end, int flags) {
	// TODO : pass the address space to the driver
	(void)space;
	mutex_acquire(&seg->lock);
	if (!seg->ops || !seg->ops->msync) {
		mutex_release(&seg->lock);
		return 0;
	}
	// cap start/end
	if (start < seg->start) start = seg->start;
	if (end > seg->end) end = seg->end;
	kassert(start < end);
	int ret = seg->ops->msync(seg, start, end, flags);
	mutex_release(&seg->lock);
	return ret;
}

static int vmm_space_raw_sync_range(vmm_space_t *space, uintptr_t start, uintptr_t end, int flags) {
	for (avltree_node_t *node = avltree_first_superior_or_equal(&space->segs, &start); node; node = avltree_next(&space->segs, node)) {
		vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
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
		if (page == PAGE_INVALID) continue;

		// do not touch ref count of IO mapping
		// because they actually do not have a ref count
		if (!(seg->flags & VMM_FLAG_IO)) {
			pmm_retain(page);
		}
		rcu_release_read();

		child_entry = mmu_create_next_entry_and_acquire_lock(child->addrspace, vaddr, child_entry, old_vaddr);
		old_vaddr = vaddr;
		if (!child_entry) {
			// release already mapped pages
			if (seg->flags & VMM_FLAG_IO) {
				mmu_unmap_pages(child->addrspace, seg->start, vaddr - seg->start);
			} else {
				mmu_unmap_and_release_pages(child->addrspace, seg->start, vaddr - seg->start);
			}
			mutex_release(&seg->lock);
			slab_free(seg);
			return -ENOMEM;
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
			mmu_entry_clear_flags(parent->addrspace, entry, MMU_FLAG_WRITE);
		}
		rcu_release_read();
		if (parent == &get_current_proc()->vmm_space) mmu_invalidate(seg->start, VMM_SIZE(seg));
	}

	avltree_insert(&child->segs, &new_seg->node);

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
	int ret = 0;
	avltree_foreach (node, &parent->segs) {
		vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
		ret = vmm_clone_seg(parent, child, seg);
		if (ret < 0) break;
	}
	rwsem_release_read(&parent->lock);
	if (ret < 0) vmm_space_unmap_all(child);
	return ret;
}
