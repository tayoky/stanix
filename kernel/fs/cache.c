#include <kernel/userspace.h>
#include <kernel/hashmap.h>
#include <kernel/string.h>
#include <kernel/assert.h>
#include <kernel/signal.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/cache.h>
#include <kernel/cond.h>
#include <kernel/vmm.h>
#include <kernel/vfs.h>
#include <kernel/pmm.h>

void init_cache(cache_t *cache) {
	memset(cache, 0, sizeof(cache_t));
	init_hashmap(&cache->pages, 256);
}

void free_cache(cache_t *cache) {
	hashmap_foreach(offset, element, &cache->pages) {
		uintptr_t page = (uintptr_t)element;
		if (cache->ops->write) cache->ops->write(cache, offset, PAGE_SIZE, NULL, NULL);
		pmm_free_page(page);
	}
	free_hashmap(&cache->pages);
}

static void set_condition(cache_t *cache, void *arg) {
	(void)cache;
	cond_t *cond = arg;
	cond_set(cond, 1);
}

static void release_pages_in_range(cache_t *cache, uintptr_t start, uintptr_t end) {
	for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		if (page == PAGE_INVALID) continue;
		pmm_free_page(page);
	}
}

void cache_read_terminate(cache_t *cache, off_t offset, size_t size) {
	uintptr_t end = offset + size;
	for (uintptr_t addr=offset; addr<end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		atomic_fetch_or(&pmm_page_info(page)->flags, PAGE_FLAG_READY);
		pmm_free_page(page);
	}
}

void cache_write_terminate(cache_t *cache, off_t offset, size_t size, cache_callback_t callback, void *arg) {
	release_pages_in_range(cache, offset, offset + size);
	cache_call_callback(cache, callback, arg);
}

int cache_cache_async(cache_t *cache, off_t offset, size_t size) {
	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end  = PAGE_ALIGN_UP(offset + size);

	if (!cache->ops || !cache->ops->read) return -EINVAL;

	uintptr_t batch_start = start;
	for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		if (page != PAGE_INVALID) {
			// the page is already cached
			// there is nothing to cache
			if (batch_start != addr) {
				cache->ops->read(cache, batch_start, addr - batch_start);
			}
			batch_start = addr + PAGE_SIZE;
			continue;
		}

		page = pmm_allocate_page();
		if (page == PAGE_INVALID) return -ENOMEM;
		pmm_page_info(page)->flags = 0;
		int have_interrupt;

		rwlock_acquire_write(&cache->lock, &have_interrupt);
		if (cache_get_page(cache, addr) == PAGE_INVALID) {
			hashmap_add(&cache->pages, (long)addr, (void*)page);
			// prevent it from being freed before we populate it
			pmm_retain(page);
		} else {
			// we lost a race
			pmm_free_page(page);
		}
		rwlock_release_write(&cache->lock, &have_interrupt);
	}

	int ret = 0;
	if (batch_start != end) {
		ret = cache->ops->read(cache, batch_start, end - batch_start);
	}
	
	if (ret < 0) {
		for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
			uintptr_t page = cache_get_page(cache, addr);
			hashmap_remove(&cache->pages, addr);
			pmm_free_page(page);
		}
		return ret;
	}
	return 0;
}

int cache_cache(cache_t *cache, off_t offset, size_t size) {
	int ret = cache_cache_async(cache, offset, size);
	if (ret < 0) return ret;
	// TODO : wait until pages are marked as ready
	return 0;
}

typedef struct uncache_req {
	off_t offset;
	size_t size;
	cache_callback_t callback;
	void *arg;
} uncache_req_t;

static void uncache_callback(cache_t *cache, void *arg) {
	uncache_req_t *req = arg;

	uintptr_t start = PAGE_ALIGN_DOWN(req->offset);
	uintptr_t end  = PAGE_ALIGN_UP(req->offset + req->size);
	
	int have_interrupt;
	rwlock_acquire_write(&cache->lock, &have_interrupt);
	for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		if (page == PAGE_INVALID) {
			// the page is not cached
			// there is nothing to uncache
			continue;
		}
		hashmap_remove(&cache->pages, addr);
		pmm_free_page(page);
	}
	rwlock_release_write(&cache->lock, &have_interrupt);
	cache_callback_t callback = req->callback;
	void *callback_arg = req->arg;
	kfree(req);
	if (callback) callback(cache, callback_arg);
}

int cache_uncache_async(cache_t *cache, off_t offset, size_t size, cache_callback_t callback, void *arg) {
	uncache_req_t *req = kmalloc(sizeof(uncache_req_t));
	req->offset   = offset;
	req->size     = size;
	req->callback = callback;
	req->arg      = arg;
	int ret = cache_flush_async(cache, offset, size, uncache_callback, req);
	if (ret < 0) kfree(req);
	return ret;
}

int cache_uncache(cache_t *cache, off_t offset, size_t size) {
	cond_t condition;
	init_cond(&condition);

	cond_set(&condition, 0);
	int ret = cache_uncache_async(cache, offset, size, set_condition, &condition);
	if (ret < 0) return ret;
	return cond_wait_interruptible(&condition, 1);	
}

int cache_flush_async(cache_t *cache, off_t offset, size_t size, cache_callback_t callback, void *arg) {
	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end  = PAGE_ALIGN_UP(offset + size);

	if (!cache->ops || !cache->ops->write) return -EINVAL;

	uintptr_t batch_start = start;
	for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
		int have_interrupt;
		rwlock_acquire_read(&cache->lock, &have_interrupt);
		uintptr_t page = cache_get_page(cache, addr);
		// prevent the page from being freed
		pmm_retain(page);
		rwlock_release_read(&cache->lock, &have_interrupt);
		long flags = 0;
		if (page != PAGE_INVALID) {
			flags = atomic_fetch_and(&pmm_page_info(page)->flags, ~PAGE_FLAG_DIRTY);
		}

		if (!(flags & PAGE_FLAG_DIRTY)) {
			pmm_free_page(page);
			if (batch_start != addr) {
				// we reached end of the batch
				cache->ops->write(cache, batch_start, addr - batch_start, callback, arg);
			}
			batch_start = addr + PAGE_SIZE;
		}
	}
	
	if (batch_start != end) {
		cache->ops->write(cache, batch_start, end - batch_start, callback, arg);
	}
	return 0;
}

int cache_flush(cache_t *cache, off_t offset, size_t size) {
	cond_t condition;
	init_cond(&condition);

	cond_set(&condition, 0);
	int ret = cache_flush_async(cache, offset, size, set_condition, &condition);
	if (ret < 0) return ret;
	return cond_wait_interruptible(&condition, 1);	
}

// mapping cache

static int cache_vmm_msync(vmm_seg_t *seg, uintptr_t start, uintptr_t end, int flags) {
	(void)flags;
	// never sync private mappings
	if (seg->flags & VMM_FLAG_PRIVATE) return 0;
	
	for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
		uintptr_t page = mmu_virt2phys((void*)addr);
		long mmu_flags = mmu_get_flags(get_current_proc()->vmm_space.addrspace, addr);
		if (mmu_flags & MMU_FLAG_DIRTY) {
			atomic_fetch_or(&pmm_page_info(page)->flags, PAGE_FLAG_DIRTY);
		}
	}
	return 0;
}

static int cache_vmm_fault(vmm_seg_t *seg, uintptr_t addr, long prot) {
	if (!(prot & seg->prot)) return 0;

	if (mmu_virt2phys((void*)addr) != PAGE_INVALID) {
		// the page is already mapped it's not out job
		return 0;
	}

	cache_t *cache = seg->private_data;
	uintptr_t vpage = PAGE_ALIGN_DOWN(addr);
	off_t offset = vpage - seg->start + seg->offset;

	
	int interrupt_save;
	rwlock_acquire_read(&cache->lock, &interrupt_save);
	
	uintptr_t page = cache_get_page(cache, offset);
	if (page == PAGE_INVALID) {
		// the page is not cached
		// we are cooked
		kdebugf("uncached mapped page access\n");
		send_sig_task(get_current_task(), SIGBUS);
		rwlock_release_read(&cache->lock, &interrupt_save);
		return 1;
	}

	// Copy on Write check
	long mapping_prot = seg->prot;
	if (seg->flags & VMM_FLAG_PRIVATE) {
		if (prot == MMU_FLAG_WRITE) {
			// if we faulted for write duplicate now
			page = pmm_dup_page(page);
			if (page == PAGE_INVALID) {
				send_sig_task(get_current_task(), SIGBUS);
				rwlock_release_read(&cache->lock, &interrupt_save);
				return 1;
			}
		} else {
			mapping_prot &= ~MMU_FLAG_WRITE;
			pmm_retain(page);
		}
	} else {
		pmm_retain(page);
	}

	mmu_map_page(get_current_proc()->vmm_space.addrspace, page, vpage, mapping_prot);
	rwlock_release_read(&cache->lock, &interrupt_save);
	return 1;
}

static vmm_ops_t cache_vmm_ops = {
	.msync = cache_vmm_msync,
	.fault = cache_vmm_fault,
};

int cache_mmap(cache_t *cache, off_t offset, vmm_seg_t *seg) {
	if (offset % PAGE_SIZE) return -EINVAL;
	cache_cache_async(cache, offset, VMM_SIZE(seg));

	seg->ops = &cache_vmm_ops;
	seg->private_data = cache;
	
	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end  = PAGE_ALIGN_UP(offset + VMM_SIZE(seg));
	uintptr_t vaddr = seg->start;

	// Copy on Write check
	long prot = seg->prot;
	if (seg->flags & VMM_FLAG_PRIVATE) {
		prot &= ~MMU_FLAG_WRITE;
	}
	
	int interrupt_save;
	rwlock_acquire_read(&cache->lock, &interrupt_save);
	for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		if (page == PAGE_INVALID) continue;
		pmm_retain(page);
		mmu_map_page(get_current_proc()->vmm_space.addrspace, page, vaddr, prot);
		vaddr += PAGE_SIZE;
	}
	rwlock_release_read(&cache->lock, &interrupt_save);
	return 0;
}

ssize_t cache_read(cache_t *cache, void *buffer, off_t offset, size_t size) {
	if ((size_t)offset >= cache->size) return 0;
	if (offset + size > cache->size) size = cache->size - offset;

	int ret = cache_cache(cache, offset, size);
	if (ret < 0) return ret;

	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end  = PAGE_ALIGN_UP(offset + size);

	int interrupt_save;
	rwlock_acquire_read(&cache->lock, &interrupt_save);
	char *buf = buffer;
	for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
		uintptr_t phys = cache_get_page(cache, addr);
		if (phys == PAGE_INVALID) return -EIO;
		uintptr_t page_start = 0;
		uintptr_t page_end   = PAGE_SIZE;
		if (addr == start) {
			page_start = offset % PAGE_SIZE;
		}
		if (addr == end - PAGE_SIZE) {
			page_end = (offset + size) % PAGE_SIZE;
			if (page_end == 0) page_end = PAGE_SIZE;
		}
		if (safe_copy_to(buf, (void*)(kernel->hhdm + phys + page_start), page_end - page_start) < 0) {
			rwlock_release_read(&cache->lock, &interrupt_save);
			return -EFAULT;
		}
		buf += page_end - page_start;
	}
	rwlock_release_read(&cache->lock, &interrupt_save);
	return size;
}

ssize_t cache_write(cache_t *cache, const void *buffer, off_t offset, size_t size) {
	if ((size_t)offset >= cache->size) return 0;
	if (offset + size > cache->size) size = cache->size - offset;

	int ret = cache_cache(cache, offset, size);
	if (ret < 0) return ret;

	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end  = PAGE_ALIGN_UP(offset + size);
	int interrupt_save;
	rwlock_acquire_read(&cache->lock, &interrupt_save);
	const char *buf = buffer;
	for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		if (page == PAGE_INVALID) {
			rwlock_release_read(&cache->lock, &interrupt_save);
			return -EIO;
		}
		uintptr_t page_start = 0;
		uintptr_t page_end   = PAGE_SIZE;
		if (addr == start) {
			page_start = offset % PAGE_SIZE;
		}
		if (addr == end - PAGE_SIZE) {
			page_end = (offset + size) % PAGE_SIZE;
			if (page_end == 0) page_end = PAGE_SIZE;
		}
		if (safe_copy_from((void*)(kernel->hhdm + page + page_start), buf, page_end - page_start) < 0) {
			rwlock_release_read(&cache->lock, &interrupt_save);
			return -EFAULT;
		}

		// mark as dirty
		atomic_fetch_or(&pmm_page_info(page)->flags, PAGE_FLAG_DIRTY);

		buf += page_end - page_start;
	}
	rwlock_release_read(&cache->lock, &interrupt_save);
	return size;
}

int cache_truncate(cache_t *cache, size_t size) {
	// TODO : free pages after the truncate
	cache->size = size;
	return 0;
}

// vfs support

static ssize_t cache_ops_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	cache_t *cache = fd->private;
	return cache_read(cache, buffer, offset, count);
}

static ssize_t cache_ops_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	cache_t *cache = fd->private;
	return cache_write(cache, buffer, offset, count);
}

static int cache_ops_ioctl(vfs_fd_t *fd, long req, void *arg) {
	cache_t *cache = fd->private;
	if (!cache->ops || !cache->ops->ioctl) return -EINVAL;
	return cache->ops->ioctl(cache, req, arg);
}

static int cache_ops_mmap(vfs_fd_t *fd, off_t offset, vmm_seg_t *seg) {
	cache_t *cache = fd->private;
	return cache_mmap(cache, offset, seg);
}

static vfs_fd_ops_t cache_ops = {
	.read  = cache_ops_read,
	.write = cache_ops_write,
	.ioctl = cache_ops_ioctl,
	.mmap  = cache_ops_mmap,
};

int cache_open(cache_t *cache, vfs_fd_t *fd) {
	fd->private = cache;
	fd->ops = &cache_ops;
	return 0;
}
