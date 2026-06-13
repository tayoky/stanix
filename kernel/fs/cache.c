#include <kernel/assert.h>
#include <kernel/cache.h>
#include <kernel/cond.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <kernel/print.h>
#include <kernel/signal.h>
#include <kernel/string.h>
#include <kernel/userspace.h>
#include <kernel/vfs.h>
#include <kernel/vmm.h>
#include <kernel/xarray.h>

typedef struct page_lru_list {
	uintptr_t first;
	uintptr_t last;
} page_lru_list_t;

static page_lru_list_t lru_lists[4] = {
	{PAGE_INVALID, PAGE_INVALID},
	{PAGE_INVALID, PAGE_INVALID},
	{PAGE_INVALID, PAGE_INVALID},
	{PAGE_INVALID, PAGE_INVALID},
};
static spinlock_t lru_lock;

#define LRU_INACTIVE 0
#define LRU_ACTIVE   2
#define LRU_CLEAN    0
#define LRU_DIRTY    1

static uintptr_t cached_page_get_lru_prev(page_t *page_info) {
	return (uintptr_t)page_info->cached.lru_prev * PAGE_SIZE;
}

static uintptr_t cached_page_get_lru_next(page_t *page_info) {
	return (uintptr_t)page_info->cached.lru_next * PAGE_SIZE;
}

static void cached_page_set_lru_prev(page_t *page_info, uintptr_t prev) {
	page_info->cached.lru_prev = prev / PAGE_SIZE;
}

static void cached_page_set_lru_next(page_t *page_info, uintptr_t next) {
	page_info->cached.lru_next = next / PAGE_SIZE;
}

static uintptr_t cached_page_get_offset(page_t *page_info) {
	return (uintptr_t)page_info->cached.offset * PAGE_SIZE;
}

static int cached_page_is_active(page_t *page_info) {
	return atomic_load(&page_info->ref_count) > 1;
}

static int cached_page_is_dirty(page_t *page_info) {
	return atomic_load(&page_info->flags) & PAGE_FLAG_DIRTY;
}

static page_lru_list_t *get_lru_list(page_t *page_info) {
	size_t index = 0;
	if (cached_page_is_active(page_info)) index += LRU_ACTIVE;
	if (cached_page_is_dirty((page_info))) index += LRU_DIRTY;
	return &lru_lists[index];
}

static void cached_page_remove_lru(page_t *page_info) {
	page_lru_list_t *lru_list = get_lru_list(page_info);
	uintptr_t prev            = cached_page_get_lru_prev(page_info);
	uintptr_t next            = cached_page_get_lru_next(page_info);
	if (prev != PAGE_INVALID) {
		page_t *prev_info = pmm_page_info(prev);
		cached_page_set_lru_next(prev_info, next);
	} else {
		lru_list->first = next;
	}
	if (next != PAGE_INVALID) {
		page_t *next_info = pmm_page_info(next);
		cached_page_set_lru_prev(next_info, prev);
	} else {
		lru_list->last = prev;
	}
}

static void cached_page_add_lru(uintptr_t page, page_t *page_info) {
	page_lru_list_t *lru_list = get_lru_list(page_info);
	cached_page_set_lru_prev(page_info, PAGE_INVALID);
	cached_page_set_lru_next(page_info, lru_list->first);
	if (lru_list->first != PAGE_INVALID) {
		page_t *next_info = pmm_page_info(lru_list->first);
		cached_page_set_lru_prev(next_info, page);
	} else {
		lru_list->last = page;
	}
	lru_list->first = page;
}

static void cached_page_mark_dirty(uintptr_t page, page_t *page_info) {
	spinlock_acquire(&lru_lock);
	if (!(atomic_fetch_or(&page_info->flags, PAGE_FLAG_DIRTY) & MMU_FLAG_DIRTY)) {
		cached_page_remove_lru(page_info);
		cached_page_add_lru(page, page_info);
	}
	spinlock_release(&lru_lock);
}

static int cached_page_clear_dirty(uintptr_t page, page_t *page_info) {
	spinlock_acquire(&lru_lock);
	int ret = atomic_fetch_or(&page_info->flags, ~PAGE_FLAG_DIRTY) & PAGE_FLAG_DIRTY;
	if (ret) {
		cached_page_remove_lru(page_info);
		cached_page_add_lru(page, page_info);
	}
	spinlock_release(&lru_lock);
	return ret;
}

static void cached_page_free(uintptr_t page) {
	page_t *page_info = pmm_page_info(page);
	spinlock_acquire(&lru_lock);
	cached_page_remove_lru(page_info);
	spinlock_release(&lru_lock);
	pmm_release_page(page);
}

void init_cache(cache_t *cache) {
	memset(cache, 0, sizeof(cache_t));
	xarray_init(&cache->pages);
}

void free_cache(cache_t *cache) {
	xarray_foreach (offset, value, &cache->pages) {
		uintptr_t page = (uintptr_t)value;
		if (cache->ops->write) cache->ops->write(cache, offset, PAGE_SIZE, NULL, NULL);
		cached_page_free(page);
	}
	xarray_destroy(&cache->pages);
}

static void set_condition(cache_t *cache, void *arg) {
	(void)cache;
	cond_t *cond = arg;
	cond_set(cond, 1);
}

static void release_pages_in_range(cache_t *cache, uintptr_t start, uintptr_t end) {
	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		if (page == PAGE_INVALID) continue;
		pmm_release_page(page);
	}
}

static int wait_page_non_busy(uintptr_t page) {
	return pmm_wait(page, PAGE_FLAG_BUSY, 0);
}

static int wait_pages_non_busy(cache_t *cache, uintptr_t start, uintptr_t end) {
	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		if (page == PAGE_INVALID) continue;
		int ret = wait_page_non_busy(page);
		if (ret < 0) return ret;
	}
	return 0;
}

static uintptr_t cache_get_page_and_clear(cache_t *cache, off_t offset) {
	uintptr_t page = (uintptr_t)xarray_clear(&cache->pages, offset);
	if (!page) return PAGE_INVALID;
	return page;
}

uintptr_t cache_evict(void) {
	// for now only evict inactive pages
	uintptr_t page = lru_lists[LRU_INACTIVE + LRU_CLEAN].last;
	if (page == PAGE_INVALID) return PAGE_INVALID;
	page_t *page_info = pmm_page_info(page);
	cache_t *cache    = page_info->private;
	off_t offset      = cached_page_get_offset(page_info);
	if (cache_uncache(cache, offset, PAGE_SIZE) < 0) return PAGE_INVALID;
	return page;
}

void cache_read_terminate(cache_t *cache, off_t offset, size_t size) {
	uintptr_t end = offset + size;
	for (uintptr_t addr = offset; addr < end; addr += PAGE_SIZE) {
		uintptr_t page    = cache_get_page(cache, addr);
		page_t *page_info = pmm_page_info(page);
		atomic_fetch_and(&page_info->flags, ~PAGE_FLAG_BUSY);
		spinlock_acquire(&lru_lock);
		cached_page_add_lru(page, page_info);
		spinlock_release(&lru_lock);
		pmm_release_page(page);
	}
}

void cache_write_terminate(cache_t *cache, off_t offset, size_t size, cache_callback_t callback, void *arg) {
	release_pages_in_range(cache, offset, offset + size);
	cache_call_callback(cache, callback, arg);
}

int cache_cache_async(cache_t *cache, off_t offset, size_t size) {
	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end   = PAGE_ALIGN_UP(offset + size);

	if (!cache->ops || !cache->ops->read) return -EINVAL;

	uintptr_t batch_start = start;
	int ret = 0;
	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
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
		if (page == PAGE_INVALID) {
			ret = -ENOMEM;
			goto error;
		}
		page_t *page_info = pmm_page_info(page);
		page_info->flags &= ~(PAGE_FLAG_DIRTY);
		page_info->flags |= PAGE_FLAG_BUSY;
		page_info->private       = cache;
		page_info->cached.offset = addr / PAGE_SIZE;

		// FIXME : we might have a race here
		if (cache_get_page(cache, addr) == PAGE_INVALID) {
			xarray_set(&cache->pages, addr, (void *)page);
			// prevent it from being freed before we populate it
			pmm_retain(page);
		} else {
			// we lost a race
			pmm_release_page(page);
		}
	}

	if (batch_start != end) {
		ret = cache->ops->read(cache, batch_start, end - batch_start);
	}

	if (ret < 0) {
error:
		// FIXME : only free pages that were busy
		for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
			uintptr_t page = cache_get_page(cache, addr);
			xarray_clear(&cache->pages, addr);
			pmm_release_page(page);
		}
		return ret;
	}
	return 0;
}

int cache_cache(cache_t *cache, off_t offset, size_t size) {
	int ret = cache_cache_async(cache, offset, size);
	if (ret < 0) return ret;
	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end   = PAGE_ALIGN_UP(offset + size);
	return wait_pages_non_busy(cache, start, end);
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
	uintptr_t end   = PAGE_ALIGN_UP(req->offset + req->size);

	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page_and_clear(cache, addr);
		if (page == PAGE_INVALID) {
			// the page is not cached
			// there is nothing to uncache
			continue;
		}
		cached_page_free(page);
	}
	cache_callback_t callback = req->callback;
	void *callback_arg        = req->arg;
	kfree(req);
	if (callback) callback(cache, callback_arg);
}

int cache_uncache_async(cache_t *cache, off_t offset, size_t size, cache_callback_t callback, void *arg) {
	uncache_req_t *req = kmalloc(sizeof(uncache_req_t));
	req->offset        = offset;
	req->size          = size;
	req->callback      = callback;
	req->arg           = arg;
	int ret            = cache_flush_async(cache, offset, size, uncache_callback, req);
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
	uintptr_t end   = PAGE_ALIGN_UP(offset + size);

	if (!cache->ops || !cache->ops->write) return -EINVAL;

	uintptr_t batch_start = start;
	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
		rcu_acquire_read(&cache->pages.rcu);
		uintptr_t page = cache_get_page(cache, addr);

		// prevent the page from being freed
		pmm_retain(page);
		rcu_release_read(&cache->pages.rcu);
		int is_dirty = 0;
		if (page != PAGE_INVALID) {
			is_dirty = cached_page_clear_dirty(page, pmm_page_info(page));
		}

		if (!is_dirty) {
			pmm_release_page(page);
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

	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
		uintptr_t page = mmu_virt2phys((void *)addr);
		long mmu_flags = mmu_get_flags(get_current_proc()->vmm_space.addrspace, addr);
		if (mmu_flags & MMU_FLAG_DIRTY) {
			atomic_fetch_or(&pmm_page_info(page)->flags, PAGE_FLAG_DIRTY);
		}
	}
	return 0;
}

static int cache_vmm_fault(vmm_seg_t *seg, uintptr_t addr, long prot) {
	if (!(prot & seg->prot)) return 0;

	if (mmu_virt2phys((void *)addr) != PAGE_INVALID) {
		// the page is already mapped it's not out job
		return 0;
	}

	cache_t *cache  = seg->private_data;
	uintptr_t vpage = PAGE_ALIGN_DOWN(addr);
	off_t offset    = vpage - seg->start + seg->offset;


	rcu_acquire_read(&cache->pages.rcu);
	uintptr_t page = cache_get_page(cache, offset);
	if (page == PAGE_INVALID) {
		// the page is not cached
		// we are cooked
		kdebugf("uncached mapped page access\n");
		send_sig_task(get_current_task(), SIGBUS);
		rcu_release_read(&cache->pages.rcu);
		return 1;
	}

	// we need to write until the page is ready
	while (wait_page_non_busy(page) == -EINTR);

	// Copy on Write check
	long mapping_prot = seg->prot;
	if (seg->flags & VMM_FLAG_PRIVATE) {
		if (prot == MMU_FLAG_WRITE) {
			// if we faulted for write duplicate now
			page = pmm_dup_page(page);
			if (page == PAGE_INVALID) {
				send_sig_task(get_current_task(), SIGBUS);
				rcu_release_read(&cache->pages.rcu);
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
	rcu_release_read(&cache->pages.rcu);
	return 1;
}

static vmm_ops_t cache_vmm_ops = {
	.msync = cache_vmm_msync,
	.fault = cache_vmm_fault,
};

int cache_mmap(cache_t *cache, off_t offset, vmm_seg_t *seg) {
	if (offset % PAGE_SIZE) return -EINVAL;
	cache_cache_async(cache, offset, VMM_SIZE(seg));

	seg->ops          = &cache_vmm_ops;
	seg->private_data = cache;

	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end   = PAGE_ALIGN_UP(offset + VMM_SIZE(seg));
	uintptr_t vaddr = seg->start;

	// Copy on Write check
	long prot = seg->prot;
	if (seg->flags & VMM_FLAG_PRIVATE) {
		prot &= ~MMU_FLAG_WRITE;
	}

	rcu_acquire_read(&cache->pages.rcu);
	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		if (page == PAGE_INVALID) continue;
		pmm_retain(page);
		mmu_map_page(get_current_proc()->vmm_space.addrspace, page, vaddr, prot);
		vaddr += PAGE_SIZE;
	}
	rcu_release_read(&cache->pages.rcu);
	return 0;
}

ssize_t cache_read(cache_t *cache, void *buffer, off_t offset, size_t size) {
	if ((size_t)offset >= cache->size) return 0;
	if (offset + size > cache->size) size = cache->size - offset;

	int ret = cache_cache(cache, offset, size);
	if (ret < 0) return ret;

	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end   = PAGE_ALIGN_UP(offset + size);

	rcu_acquire_read(&cache->pages.rcu);
	char *buf = buffer;
	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
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
		if (safe_copy_to(buf, mmu_phys2virt(phys + page_start), page_end - page_start) < 0) {
			rcu_release_read(&cache->pages.rcu);
			return -EFAULT;
		}
		buf += page_end - page_start;
	}
	rcu_release_read(&cache->pages.rcu);
	return size;
}

ssize_t cache_write(cache_t *cache, const void *buffer, off_t offset, size_t size) {
	if ((size_t)offset >= cache->size) return 0;
	if (offset + size > cache->size) size = cache->size - offset;

	int ret = cache_cache(cache, offset, size);
	if (ret < 0) return ret;

	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end   = PAGE_ALIGN_UP(offset + size);

	rcu_acquire_read(&cache->pages.rcu);
	const char *buf = buffer;
	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		if (page == PAGE_INVALID) {
			rcu_release_read(&cache->pages.rcu);
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
		if (safe_copy_from(mmu_phys2virt(page + page_start), buf, page_end - page_start) < 0) {
			rcu_release_read(&cache->pages.rcu);
			return -EFAULT;
		}

		cached_page_mark_dirty(page, pmm_page_info(page));

		buf += page_end - page_start;
	}
	rcu_release_read(&cache->pages.rcu);
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
	fd->ops     = &cache_ops;
	return 0;
}
