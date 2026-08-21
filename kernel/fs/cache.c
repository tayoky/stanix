#include <kernel/assert.h>
#include <kernel/cache.h>
#include <kernel/oneshot.h>
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

static page_lru_list_t lru_active = {PAGE_INVALID, PAGE_INVALID};
static page_lru_list_t lru_inactive = {PAGE_INVALID, PAGE_INVALID};
static spinlock_t lru_lock;
static spinlock_t dirty_lock;
static list_t caches;
static list_t dirty_caches;

// TODO : implement difference between dirty and not dirty

static uintptr_t cached_page_get_lru_prev(page_t *page_info) {
	return PFN2PAGE(page_info->cached.lru_prev);
}

static uintptr_t cached_page_get_lru_next(page_t *page_info) {
	return PFN2PAGE(page_info->cached.lru_next);
}

static void cached_page_set_lru_prev(page_t *page_info, uintptr_t prev) {
	page_info->cached.lru_prev = PAGE2PFN(prev);
}

static void cached_page_set_lru_next(page_t *page_info, uintptr_t next) {
	page_info->cached.lru_next = PAGE2PFN(next);
}

static uintptr_t cached_page_get_offset(page_t *page_info) {
	return PFN2PAGE(page_info->cached.offset);
}

static int cached_page_is_active(page_t *page_info) {
	return atomic_load(&page_info->ref_count) > 1;
}

static int cached_page_is_dirty(page_t *page_info) {
	return atomic_load(&page_info->flags) & PAGE_FLAG_DIRTY;
}

static page_lru_list_t *get_lru_list(page_t *page_info) {
	(void)page_info;
	return &lru_inactive;
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

static void cache_mark_page_dirty(cache_t *cache, uintptr_t page) {
	page_t *page_info = pmm_page_info(page);
	if (!(atomic_fetch_or(&page_info->flags, PAGE_FLAG_DIRTY) & PAGE_FLAG_DIRTY)) {
		spinlock_acquire(&dirty_lock);
		if (cache->dirty_lock-- == 1) {
			// this is the first dirty page
			list_append(&dirty_caches, &cache->dirty_node);
		}
		spinlock_release(&dirty_lock);
	}
}

static int cache_clear_page_dirty(cache_t *cache, uintptr_t page) {
	page_t *page_info = pmm_page_info(page);
	int ret = atomic_fetch_and(&page_info->flags, ~PAGE_FLAG_DIRTY) & PAGE_FLAG_DIRTY;
	if (ret) {
		spinlock_acquire(&dirty_lock);
		if (cache->dirty_lock-- == 1) {
			// this was the last dirty page
			list_remove(&dirty_caches, &cache->dirty_node);
		}
		spinlock_release(&dirty_lock);
	}
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
	list_append(&caches, &cache->node);
}

void free_cache(cache_t *cache) {
	list_remove(&caches, &cache->node);
	// flush the whole thing
	cache_flush(cache, 0, cache->size);
	xarray_foreach (offset, value, &cache->pages) {
		uintptr_t page = (uintptr_t)value;
		cached_page_free(page);
	}
	xarray_destroy(&cache->pages);
}

void cache_flush_all(void) {
	// TODO : use dirty_lock
	foreach (node, &dirty_caches) {
		cache_t *cache = container_of(node, cache_t, dirty_node);
		cache_flush(cache, 0, cache->size);
	}
}

static void signal_oneshot(cache_t *cache, void *arg) {
	(void)cache;
	oneshot_t *oneshot = arg;
	oneshot_signal(oneshot);
}

static void cache_get_range(cache_t *cache, off_t offset, size_t size, uintptr_t *start, uintptr_t *end) {
	*start     = PAGE_ALIGN_DOWN(offset);
	*end       = PAGE_ALIGN_UP(offset + size);
	uintptr_t cache_end = PAGE_ALIGN_UP(cache->size);
	if (*start > cache_end) {
		*start = cache_end;
	}
	if (*end > cache_end) {
		*end = cache_end;
	}
}

static int wait_page_ready(uintptr_t page) {
	return pmm_wait(page, PAGE_FLAG_READING, 0);
}

static void *page2value(uintptr_t page) {
	if (page == PAGE_INVALID) return NULL;
	return (void*)page;
}

static uintptr_t value2page(void *value) {
	if (!value) return PAGE_INVALID;
	return (uintptr_t)value;
}

static uintptr_t cache_lookup_page_and_clear(cache_t *cache, off_t offset) {
	return value2page(xarray_clear(&cache->pages, PAGE2PFN(offset)));
}

static uintptr_t cache_compare_and_set_page(cache_t *cache, off_t offset, uintptr_t expected, uintptr_t page) {
	void *expected_value = page2value(expected);
	void *value          = page2value(page);	
	return value2page(xarray_cmpxchg(&cache->pages, PAGE2PFN(offset), expected_value, value));
}

uintptr_t cache_evict(void) {
	// for now only evict inactive pages
	uintptr_t page = lru_inactive.last;
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
		uintptr_t page    = cache_lookup_page(cache, addr);
		page_t *page_info = pmm_page_info(page);
		atomic_fetch_and(&page_info->flags, ~PAGE_FLAG_READING);
		pmm_wakeup(page);
		spinlock_acquire(&lru_lock);
		cached_page_add_lru(page, page_info);
		spinlock_release(&lru_lock);
	}
}

void cache_write_terminate(cache_t *cache, off_t offset, size_t size, cache_callback_t callback, void *arg) {
	uintptr_t end = offset + size;
	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
		uintptr_t page = cache_lookup_page(cache, addr);
		if (page == PAGE_INVALID) continue;
		page_t *page_info = pmm_page_info(page);
		atomic_fetch_and(&page_info->flags, ~PAGE_FLAG_WRITING);
		pmm_wakeup(page);
		pmm_release_page(page);
	}
	cache_call_callback(cache, callback, arg);
}

static uintptr_t setup_page(cache_t *cache, off_t offset, int *raced) {
	uintptr_t page = pmm_allocate_page();
	if (page == PAGE_INVALID) return PAGE_INVALID;
	
	page_t *page_info = pmm_page_info(page);
	page_info->flags &= ~(PAGE_FLAG_DIRTY);
	page_info->flags |= PAGE_FLAG_READING;
	page_info->private       = cache;
	page_info->cached.offset = PAGE2PFN(addr);

	uintptr_t new_page = cache_compare_and_set_page(cache, addr, PAGE_INVALID, page);
	if (new_page == PAGE_INVALID) {
		*raced = 0;
		return page;
	} else {
		// we lost a race
		pmm_release_page(page);
		*raced = 1;
		return new_page;
	}
}

uintptr_t cache_get_page(cache_t *cache, off_t offset) {
	rcu_acquire_read(&cache->pages.rcu);
	uintptr_t page = cache_lookup_page(cache, offset);
	int need_read = 0;
	if (page == PAGE_INVALID) {
		int raced;
		page = setup_page(cache, addr, &raced);
		if (!raced) {
			need_read = 1;
		}
	}
	pmm_retain(page);
	rcu_release_read(&cache->pages.rcu);
	if (need_read) {
		// we need to load the page
		cache->ops->read(cache, PAGE_ALIGN_DOWN(offset), PAGE_ALIGN_DOWN(offset) + PAGE_SIZE);
	}
	wait_page_ready(page);
	// TODO : put at the end of lru
	return page;
}

int cache_preload(cache_t *cache, off_t offset, size_t size) {
	if (!cache->ops || !cache->ops->read) return -EINVAL;

	uintptr_t start, end;
	cache_get_range(cache, offset, size, &start, &end);

	uintptr_t batch_start = start;
	int ret = 0;
	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
		uintptr_t page = cache_lookup_page(cache, addr);
		// fast path
		if (page != PAGE_INVALID) {
			rcu_release_read(&cache->pages.rcu);
already_cached:
			// the page is already cached
			// there is nothing to load
			if (batch_start != addr) {
				cache->ops->read(cache, batch_start, addr - batch_start);
			}
			batch_start = addr + PAGE_SIZE;
			continue;
		}

		int raced;
		page = setup_page(cache, addr, &raced);
		if (raced) {
			goto already_cached;
		}
	}

	if (batch_start != end) {
		ret = cache->ops->read(cache, batch_start, end - batch_start);
	}

	if (ret < 0) {
error:
		// FIXME : only free pages that were busy
		for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
			uintptr_t page = cache_lookup_page_and_clear(cache, addr);
			pmm_release_page(page);
		}
		return ret;
	}
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

	uintptr_t start, end;
	cache_get_range(cache, req->offset, req->size, &start, &end);

	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
		uintptr_t page = cache_lookup_page_and_clear(cache, addr);
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
	oneshot_t oneshot;
	oneshot_init(&oneshot);

	int ret = cache_uncache_async(cache, offset, size, signal_oneshot, &oneshot);
	if (ret < 0) return ret;
	return oneshot_wait_interruptible(&oneshot);
}

int cache_flush_async(cache_t *cache, off_t offset, size_t size, cache_callback_t callback, void *arg) {
	if (!cache->ops || !cache->ops->write) return -EINVAL;

	uintptr_t start, end;
	cache_get_range(cache, offset, size, &start, &end);

	uintptr_t batch_start = PAGE_INVALID;
	uintptr_t batch_end   = PAGE_INVALID;
	rcu_acquire_read(&cache->pages.rcu);
	cache_foreach_range(addr, page, cache, start, end) {
		if (batch_start != PAGE_INVALID && batch_end != addr) {
			// we reached end of the batch
			rcu_release_read(&cache->pages.rcu);
			cache->ops->write(cache, batch_start, batch_end - batch_start, callback, arg);
			rcu_acquire_read(&cache->pages.rcu);
			batch_start = PAGE_INVALID;
			batch_end = PAGE_INVALID;
		}

		if (!cache_clear_page_dirty(cache, page)) {
			if (batch_start != PAGE_INVALID && batch_end != addr) {
				// we reached end of the batch
				rcu_release_read(&cache->pages.rcu);
				cache->ops->write(cache, batch_start, batch_end - batch_start, callback, arg);
				rcu_acquire_read(&cache->pages.rcu);
				batch_start = PAGE_INVALID;
				batch_end = PAGE_INVALID;
			}
			continue;
		}
		if (atomic_fetch_or(&pmm_page_info(page)->flags, PMM_FLAG_WRITING) & PMM_FLAG_WRITING) {
			// already writing ???
			// what do we do
			// TODO : handle this
		}

		if (batch_start == PAGE_INVALID) batch_start = addr;
		batch_end = addr + PAGE_SIZE;

		// prevent the page from being freed while we write
		pmm_retain(page);
	}
	rcu_release_read(&cache->pages.rcu);

	if (batch_start != PAGE_INVALID) {
		cache->ops->write(cache, batch_start, batch_end - batch_start, callback, arg);
	}
	return 0;
}

int cache_flush(cache_t *cache, off_t offset, size_t size) {
	oneshot_t oneshot;
	oneshot_init(&oneshot);

	int ret = cache_flush_async(cache, offset, size, signal_oneshot, &oneshot);
	if (ret < 0) return ret;
	return oneshot_wait_interruptible(&oneshot);
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
			mmu_set_flags(get_current_proc()->vmm_space.addrspace, addr, mmu_flags & ~MMU_FLAG_DIRTY);
			cache_mark_page_dirty(cache, page);
		}
	}

	if (flags & VMM_FLAG_SYNC) {
		return cache_flush(seg->private_data, seg->offset + start - seg->start, end - start);
	} else {
		return 0;
	}
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

	uintptr_t page = cache_get_page(cache, offset);
	if (page == PAGE_INVALID) {
		// the page is not cached
		// we are cooked
		kdebugf("uncached mapped page access\n");
		signal_send_task(get_current_task(), SIGBUS);
		return 1;
	}

	// Copy on Write check
	long mapping_prot = seg->prot;
	if (seg->flags & VMM_FLAG_PRIVATE) {
		if (prot == MMU_FLAG_WRITE) {
			// if we faulted for write duplicate now
			page = pmm_dup_page(page);
			pmm_release_page(page);
			if (page == PAGE_INVALID) {
				signal_send_task(get_current_task(), SIGBUS);
				return 1;
			}
		} else {
			mapping_prot &= ~MMU_FLAG_WRITE;
		}
	}

	// cache_get_page already made a new ref to the page
	mmu_map_page(get_current_proc()->vmm_space.addrspace, page, vpage, mapping_prot);
	return 1;
}

static vmm_ops_t cache_vmm_ops = {
	.msync = cache_vmm_msync,
	.fault = cache_vmm_fault,
};

int cache_mmap(cache_t *cache, off_t offset, vmm_seg_t *seg) {
	if (offset % PAGE_SIZE) return -EINVAL;
	int ret = cache_preload(cache, offset, VMM_SIZE(seg));
	if (ret < 0) return ret;

	seg->ops          = &cache_vmm_ops;
	seg->private_data = cache;

	uintptr_t start, end;
	cache_get_range(cache, offset, VMM_SIZE(seg), &start, &end);
	uintptr_t vaddr = seg->start;

	// Copy on Write check
	long prot = seg->prot;
	if (seg->flags & VMM_FLAG_PRIVATE) {
		prot &= ~MMU_FLAG_WRITE;
	}

	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE, vaddr += PAGE_SIZE) {
		uintptr_t page = cache_lookup_and_ref_page(cache, addr);
		if (page == PAGE_INVALID) continue;
		
		mmu_map_page(get_current_proc()->vmm_space.addrspace, page, vaddr, prot);
	}
	return 0;
}

ssize_t cache_read(cache_t *cache, void *buffer, off_t offset, size_t size) {
	if ((size_t)offset >= cache->size) return 0;
	if (offset + size > cache->size) size = cache->size - offset;

	int ret = cache_preload(cache, offset, size);
	if (ret < 0) return ret;

	uintptr_t start, end;
	cache_get_range(cache, offset, size, &start, &end);

	char *buf = buffer;
	ssize_t total = 0;
	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		if (page == PAGE_INVALID) {
			ret = -EIO;
			break;
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
		if (safe_copy_to(buf, mmu_phys2virt(page + page_start), page_end - page_start) < 0) {
			ret = -EFAULT;
			break;
		}
		pmm_release_page(page);
		buf += page_end - page_start;
		total += page_end - page_start;
	}
	if (total == 0 && ret < 0) return ret;
	return total;
}

ssize_t cache_write(cache_t *cache, const void *buffer, off_t offset, size_t size) {
	if ((size_t)offset >= cache->size) return 0;
	if (offset + size > cache->size) size = cache->size - offset;

	int ret = cache_preload(cache, offset, size);
	if (ret < 0) return ret;

	uintptr_t start, end;
	cache_get_range(cache, offset, size, &start, &end);

	const char *buf = buffer;
	ssize_t total = 0;
	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		if (page == PAGE_INVALID) {
			ret = -EIO;
			break;
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
			ret = -EFAULT;
			break;
		}

		cache_mark_page_dirty(cache, page);

		pmm_release_page(page);
		buf += page_end - page_start;
		total += page_end - page_start;
	}
	if (total == 0 && ret < 0) return ret;
	return total;
}

int cache_truncate(cache_t *cache, size_t size) {
	// FIXME : we might need a lock for this
	if (size < cache->size) {
		int ret = cache_uncache(cache, PAGE_ALIGN_UP(size), cache->size - size);
		if (ret < 0) return ret;
	}
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
