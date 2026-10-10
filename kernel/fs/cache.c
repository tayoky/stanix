#include <kernel/assert.h>
#include <kernel/cache.h>
#include <kernel/oneshot.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <kernel/print.h>
#include <kernel/signal.h>
#include <kernel/string.h>
#include <kernel/userspace.h>
#include <kernel/mutex.h>
#include <kernel/vfs.h>
#include <kernel/vmm.h>
#include <kernel/xarray.h>

// a list of TODOes
// - 1 implement page tiers
// - 2 implement a periodic scanner of ptes
// - 3 fix any other races

typedef struct page_list {
	uintptr_t first;
	uintptr_t last;
} page_list_t;

#define GENERATIONS_COUNT 4

size_t base_generation = 0;    // protected by lru_lock
size_t current_generation = 3; // protected by lru_lock
static page_list_t generations[GENERATIONS_COUNT] = {
	{PAGE_INVALID, PAGE_INVALID},
	{PAGE_INVALID, PAGE_INVALID},
	{PAGE_INVALID, PAGE_INVALID},
	{PAGE_INVALID, PAGE_INVALID},
}; // protected by lru_lock
static spinlock_t lru_lock;
static spinlock_t dirty_lock;
static list_t caches;
static list_t dirty_caches;
static ATOMIC(ssize_t) dirty_pages;

static size_t cached_page_get_gen(page_t *page_info) {
	return (atomic_load(&page_info->flags) & PAGE_FLAG_GEN) >> PAGE_FLAG_GEN_SHIFT;
}

static void cached_page_set_gen(page_t *page_info, size_t gen) {
	spinlock_assert_acquired(&lru_lock);
	atomic_fetch_and(&page_info->flags, ~PAGE_FLAG_GEN);
	atomic_fetch_or(&page_info->flags, (gen << PAGE_FLAG_GEN_SHIFT) & PAGE_FLAG_GEN);
}

static uintptr_t cached_page_get_prev(page_t *page_info) {
	return PFN2PAGE(page_info->cached.prev);
}

static uintptr_t cached_page_get_next(page_t *page_info) {
	return PFN2PAGE(page_info->cached.next);
}

static void cached_page_set_prev(page_t *page_info, uintptr_t prev) {
	page_info->cached.prev = PAGE2PFN(prev);
}

static void cached_page_set_next(page_t *page_info, uintptr_t next) {
	page_info->cached.next = PAGE2PFN(next);
}

static uintptr_t cached_page_get_offset(page_t *page_info) {
	return PFN2PAGE(page_info->cached.offset);
}

static int cached_page_is_dirty(page_t *page_info) {
	return atomic_load(&page_info->flags) & PAGE_FLAG_DIRTY;
}

static int cached_page_is_evicted(page_t *page_info) {
	return page_info->private == NULL;
}

static void cached_page_add_to_list(page_list_t *list, uintptr_t page, page_t *page_info) {
	cached_page_set_prev(page_info, PAGE_INVALID);
	cached_page_set_next(page_info, list->first);
	if (list->first != PAGE_INVALID) {
		page_t *next_info = pmm_page_info(list->first);
		cached_page_set_prev(next_info, page);
	} else {
		list->last = page;
	}
	list->first = page;
}

static void cached_page_remove_from_list(page_list_t *list, page_t *page_info) {
	uintptr_t prev = cached_page_get_prev(page_info);
	uintptr_t next = cached_page_get_next(page_info);
	if (prev != PAGE_INVALID) {
		page_t *prev_info = pmm_page_info(prev);
		cached_page_set_next(prev_info, next);
	} else {
		list->first = next;
	}
	if (next != PAGE_INVALID) {
		page_t *next_info = pmm_page_info(next);
		cached_page_set_prev(next_info, prev);
	} else {
		list->last = prev;
	}
}

static void cached_page_add_to_gen(uintptr_t page, page_t *page_info, size_t gen) {
	spinlock_assert_acquired(&lru_lock);
	page_list_t *lru_list = &generations[gen];
	cached_page_set_gen(page_info, gen);
	cached_page_add_to_list(lru_list, page, page_info);
}

static void cached_page_remove_from_gen(page_t *page_info) {
	spinlock_assert_acquired(&lru_lock);
	size_t gen = cached_page_get_gen(page_info);
	page_list_t *lru_list = &generations[gen];
	cached_page_remove_from_list(lru_list, page_info);
}

static void cached_page_move_to_gen(uintptr_t page, page_t *page_info, size_t gen) {
	spinlock_assert_acquired(&lru_lock);
	cached_page_remove_from_gen(page_info);
	cached_page_add_to_gen(page, page_info, gen);
}

static void cache_mark_page_dirty(cache_t *cache, uintptr_t page) {
	page_t *page_info = pmm_page_info(page);
	if (!(atomic_fetch_or(&page_info->flags, PAGE_FLAG_DIRTY) & PAGE_FLAG_DIRTY)) {
		atomic_fetch_add(&dirty_pages, 1);
		spinlock_acquire(&dirty_lock);
		if (cache->dirty_count++ == 0) {
			// this is the first dirty page
			list_append(&dirty_caches, &cache->dirty_node);
		}
		spinlock_release(&dirty_lock);
	}
}

static void cache_mark_page_active(cache_t *cache, uintptr_t page) {
	(void)cache;
	atomic_fetch_or(&pmm_page_info(page)->flags, PAGE_FLAG_ACTIVE);
}

static int cache_clear_page_dirty(cache_t *cache, uintptr_t page) {
	page_t *page_info = pmm_page_info(page);
	int ret = atomic_fetch_and(&page_info->flags, ~PAGE_FLAG_DIRTY) & PAGE_FLAG_DIRTY;
	if (ret) {
		atomic_fetch_sub(&dirty_pages, 1);
		spinlock_acquire(&dirty_lock);
		if (cache->dirty_count-- == 1) {
			// this was the last dirty page
			list_remove(&dirty_caches, &cache->dirty_node);
		}
		spinlock_release(&dirty_lock);
	}
	return ret;
}

static void cache_get_range(cache_t *cache, off_t offset, size_t size, uintptr_t *start, uintptr_t *end) {
	if (cache->size >= (size_t)offset && cache->size - offset < size) {
		size = cache->size - offset;
	}
	*start     = PAGE_ALIGN_DOWN(offset);
	*end       = PAGE_ALIGN_UP(offset + size);
	uintptr_t cache_end = PAGE_ALIGN_UP(cache->size);
	if (*end > cache_end) {
		*end = cache_end;
	}
}

static int cache_wait_page_ready(uintptr_t page) {
	unsigned int flags;
	int ret = pmm_wait_get(page, PAGE_FLAG_READING, 0, &flags);
	if (ret < 0) return ret;
	ret = -((flags & PAGE_FLAG_ERROR) >> PAGE_FLAG_ERROR_SHIFT);
	return ret;
}

static int cache_wait_page_written(uintptr_t page) {
	unsigned int flags;
	int ret = pmm_wait_get(page, PAGE_FLAG_WRITING, 0, &flags);
	if (ret < 0) return ret;
	ret = -((flags & PAGE_FLAG_ERROR) >> PAGE_FLAG_ERROR_SHIFT);
	return ret;
}

static int cached_page_is_doing_io(uintptr_t page) {
	return atomic_load(&pmm_page_info(page)->flags) & (PAGE_FLAG_WRITING | PAGE_FLAG_READING | PAGE_FLAG_EVICTING);
}

static int cache_wait_page_no_io(uintptr_t page) {
	unsigned int flags;
	int ret = pmm_wait_get(page, PAGE_FLAG_WRITING | PAGE_FLAG_READING | PAGE_FLAG_EVICTING, 0, &flags);
	if (ret < 0) return ret;
	ret = -((flags & PAGE_FLAG_ERROR) >> PAGE_FLAG_ERROR_SHIFT);
	return ret;
}

static void *page2value(uintptr_t page) {
	if (page == PAGE_INVALID) return NULL;
	return (void*)page;
}

static uintptr_t value2page(void *value) {
	if (!value) return PAGE_INVALID;
	return (uintptr_t)value;
}

static uintptr_t cache_lookup_and_clear_page(cache_t *cache, off_t offset) {
	return value2page(xarray_clear(&cache->pages, PAGE2PFN(offset)));
}

static uintptr_t cache_compare_and_set_page(cache_t *cache, off_t offset, uintptr_t expected, uintptr_t page) {
	void *expected_value = page2value(expected);
	void *value          = page2value(page);	
	return value2page(xarray_cmpxchg(&cache->pages, PAGE2PFN(offset), expected_value, value));
}

static void cached_page_set_error(page_t *page_info, int ret) {
	atomic_fetch_and(&page_info->flags, ~PAGE_FLAG_ERROR);
	atomic_fetch_or(&page_info->flags, ((uint16_t)-ret) << PAGE_FLAG_ERROR_SHIFT);
}

static int cached_page_remove(uintptr_t page) {
	pmm_assert_page_lock_acquired(page);
	page_t *page_info = pmm_page_info(page);
	if (cached_page_is_evicted(page_info)) {
		// already evicted
		return 0;
	}
	cache_t *cache = page_info->private;
	off_t offset = cached_page_get_offset(page_info);
	page_info->private = NULL;
	cache_lookup_and_clear_page(cache, offset);
	pmm_release_page(page);
	return 1;
}

void cache_read_terminate(cache_t *cache, pages_batch_t *pages_batch, int ret) {
	(void)cache;
	pages_batch_foreach (page, pages_batch) {
		page_t *page_info = pmm_page_info(page);
		cached_page_set_error(page_info, ret);
		
		if (ret < 0) {
			// the read failed, remove the pages
			cached_page_remove(page);
		}

		atomic_fetch_and(&page_info->flags, ~PAGE_FLAG_READING);
		pmm_wakeup(page);
		pmm_release_page(page);
	}
}

void cache_write_terminate(cache_t *cache, pages_batch_t *pages_batch, int ret) {
	pages_batch_foreach (page, pages_batch) {
		page_t *page_info = pmm_page_info(page);
		cached_page_set_error(page_info, ret);

		if (ret < 0) {
			// the write failed the page return to dirty
			cache_mark_page_dirty(cache, page);
		}

		atomic_fetch_and(&page_info->flags, ~PAGE_FLAG_WRITING);
		pmm_wakeup(page);
		pmm_release_page(page);
	}
}

/**
 * @brief setup a new page in a cache
 * @param cache the cache to setup the new page in
 * @param offset the offset of the page to setup
 * @param raced set to 1 if the page was already present
 * @return a new ref to the page
 */
static uintptr_t cache_setup_page(cache_t *cache, off_t offset, int *raced) {
	uintptr_t page = pmm_allocate_page();
	if (page == PAGE_INVALID) return PAGE_INVALID;
	
	page_t *page_info = pmm_page_info(page);
	page_info->flags &= ~(PAGE_FLAG_DIRTY | PAGE_FLAG_ACTIVE | PAGE_FLAG_WRITING | PAGE_FLAG_EVICTING | PAGE_FLAG_LOCKED);
	page_info->flags |= PAGE_FLAG_READING;
	page_info->private       = cache;
	page_info->cached.offset = PAGE2PFN(offset);

	pmm_acquire_page_lock(page);
	rcu_acquire_read(&cache->pages.rcu);
	uintptr_t new_page = cache_compare_and_set_page(cache, offset, PAGE_INVALID, page);

	if (new_page == PAGE_INVALID) {
		*raced = 0;
		pmm_retain(page);
		rcu_release_read(&cache->pages.rcu);
		
		// add to the current generation since 
		// this page is freshly new
		spinlock_acquire(&lru_lock);
		cached_page_add_to_gen(page, page_info, current_generation);
		spinlock_release(&lru_lock);
		pmm_release_page_lock(page);
		return page;
	} else {
		// we lost a race
		*raced = 1;
		pmm_retain(new_page);
		rcu_release_read(&cache->pages.rcu);

		pmm_release_page_lock(page);
		pmm_release_page(page);
		return new_page;
	}
}

static int cache_free_pages(cache_t *cache, off_t offset, size_t size) {
	rcu_acquire_read(&cache->pages.rcu);
	cache_foreach_range(addr, page, cache, offset, offset + size) {
		pmm_retain(page);
		rcu_release_read(&cache->pages.rcu);

		page_t *page_info = pmm_page_info(page);
		
		// we need to make sure the page is not being written back/evicted when we remove it
		pmm_acquire_page_lock(page);
		while (cached_page_is_doing_io(page)) {
			pmm_release_page_lock(page);
			int ret = cache_wait_page_no_io(page);
			if (ret < 0) {
				pmm_release_page(page);
				return ret;
			}
			pmm_acquire_page_lock(page);
		}
		if (cached_page_is_evicted(page_info)) {
			pmm_release_page_lock(page);
			pmm_release_page(page);
			rcu_acquire_read(&cache->pages.rcu);
			continue;
		}
		int removed_it = cached_page_remove(page);
		(void)removed_it;
		kassert(removed_it);

		spinlock_acquire(&lru_lock);
		cached_page_remove_from_gen(page_info);
		spinlock_release(&lru_lock);

		pmm_release_page(page); // the ref we created
		pmm_release_page(page); // the from from the cache
	}
	rcu_release_read(&cache->pages.rcu);
	return 0;
}

static int cache_read_pages(cache_t *cache, pages_batch_t *pages_batch) {
	if (!cache->ops || !cache->ops->read) return -EOPNOTSUPP;
	int ret = cache->ops->read(cache, pages_batch);
	if (ret < 0) {
		// synchronous error
		cache_read_terminate(cache, pages_batch, ret);
	}
	return ret;
}

static int cache_write_pages(cache_t *cache, pages_batch_t *pages_batch) {
	if (!cache->ops || !cache->ops->write) return -EOPNOTSUPP;
	int ret = cache->ops->write(cache, pages_batch);
	if (ret < 0) {
		// synchronous error
		cache_write_terminate(cache, pages_batch, ret);
	}
	return ret;
}

void init_cache(cache_t *cache) {
	memset(cache, 0, sizeof(cache_t));
	xarray_init(&cache->pages);
	mutex_init(&cache->mutex);
	list_append(&caches, &cache->node);
}

void free_cache(cache_t *cache) {
	list_remove(&caches, &cache->node);
	// flush the whole thing
	cache_flush_whole_async(cache);
	cache_free_pages(cache, 0, PAGE_ALIGN_UP(cache->size));
	xarray_destroy(&cache->pages);
}

size_t cache_evict(size_t to_evict) {
	size_t evicted_pages = 0;

	// evict the base generation
	// we use a 2 stage approach
	// - 1 grab every page we can evict and put them on a local list
	// - 2 actually evict them

	// build the list
	spinlock_acquire(&lru_lock);
	page_list_t list = {PAGE_INVALID, PAGE_INVALID};
	for (size_t i = 0; i < GENERATIONS_COUNT && to_evict > 0; i++) {
		uintptr_t next = 0;
		for (next = generations[base_generation].first; next != PAGE_INVALID && to_evict > 0;) {
			uintptr_t page = next;
			page_t *page_info = pmm_page_info(page);
			kassert(page_info);
			next = cached_page_get_next(page_info);

			if (atomic_fetch_and(&page_info->flags, ~PAGE_FLAG_ACTIVE) & PAGE_FLAG_ACTIVE) {
				// this is an active page, place it in the current generation
				cached_page_move_to_gen(page, page_info, current_generation);
				continue;
			}
			
			// this is a cold page, evict it
			if (atomic_fetch_or(&page_info->flags, PAGE_FLAG_EVICTING) & PAGE_FLAG_EVICTING) {
				// somebody else is evicting it we cannot evict it
				continue;
			}

			pmm_retain(page);
			cached_page_remove_from_gen(page_info);
			cached_page_add_to_list(&list, page, page_info);
			to_evict--;
		}
		
		// if we evicted a whole generation, age generations
		if (next == PAGE_INVALID) {
			base_generation    = (base_generation + 1) % GENERATIONS_COUNT;
			current_generation = (current_generation + 1) % GENERATIONS_COUNT;
			kdebugf("age\n");
		}
	}
	spinlock_release(&lru_lock);

	for (uintptr_t next = list.first; next != PAGE_INVALID;) {
		uintptr_t page = next;
		page_t *page_info = pmm_page_info(page);
		kassert(page_info);
		next = cached_page_get_next(page_info);
		// this is some pseudo code, is not functional and is probably unsafe
	
		pmm_acquire_page_lock(page);
		if (cached_page_is_evicted(page_info)) {
			// already evicted, we have nothing to do
			cached_page_remove_from_list(&list, page_info);
			atomic_fetch_and(&page_info->flags, ~PAGE_FLAG_EVICTING);
			pmm_release_page_lock(page);
			pmm_release_page(page);
			continue;
		}

		if (cached_page_is_dirty(page_info)) {
			cache_t *cache = page_info->private;
			pmm_release_page_lock(page);

			// TODO : maybee don't flush one page at time
			while (atomic_fetch_or(&page_info->flags, PAGE_FLAG_WRITING) & PAGE_FLAG_WRITING) {
				// already writing
				// wait until write complete
				cache_wait_page_written(page);
			}
			pages_batch_t pages_batch;
			pages_batch_from_page(&pages_batch, page);
			if (cache_write_pages(cache, &pages_batch) < 0) continue;
			if (cache_wait_page_written(page) < 0) continue;

			pmm_acquire_page_lock(page);
		}

		cached_page_remove_from_list(&list, page_info);

		// it's not possible that somebody else freed it because of the EVICTING bit
		int removed_it = cached_page_remove(page);
		(void)removed_it;
		kassert(removed_it);
		atomic_fetch_and(&page_info->flags, ~PAGE_FLAG_EVICTING);
		pmm_release_page_lock(page);
		pmm_release_page(page);
		evicted_pages++;
	}

	// put back pages we were not able to evict
	if (list.first != PAGE_INVALID) {
		spinlock_acquire(&lru_lock);
		for (uintptr_t next = list.first; next != PAGE_INVALID;) {
			uintptr_t page = next;
			page_t *page_info = pmm_page_info(page);
			kassert(page_info);
			next = cached_page_get_next(page_info);

			cached_page_add_to_gen(page, page_info, base_generation);
			atomic_fetch_and(&page_info->flags, ~PAGE_FLAG_EVICTING);
			pmm_wakeup(page);
			pmm_release_page(page);
		}
		spinlock_release(&lru_lock);
	}
	kdebugf("evicted %zu pages\n", evicted_pages);
	return evicted_pages;
}

size_t cache_get_dirty_pages(void) {
	ssize_t ret = atomic_load(&dirty_pages);
	if (ret < 0) {
		// the dirty pages counter didn't keep up
		// it's just 0 and the counter will catch up soon or late
		return 0;
	}
	return ret;
}

size_t cache_get_clean_pages(void) {
	return pmm_get_usable_pages() - cache_get_dirty_pages();
}

size_t cache_get_clean_pages_threshold(void) {
	// TODO : make this configurable
	return 10 * 1000000 / PAGE_SIZE;
}

int cache_flush_whole_async(cache_t *cache) {
	return cache_flush_async(cache, 0, cache->size);
}

int cache_flush_whole(cache_t *cache) {
	return cache_flush(cache, 0, cache->size);
}

void cache_flush_all(void) {
	for (;;) {
		spinlock_acquire(&dirty_lock);
		if (list_is_empty(&dirty_caches)) {
			spinlock_release(&dirty_lock);
			break;
		}
		cache_t *cache = container_of(dirty_caches.first_node, cache_t, dirty_node);
		list_remove(&dirty_caches, &cache->dirty_node);
		spinlock_release(&dirty_lock);

		// FIXME RACE : how do we make sure the cache hasen't been freed?
		cache_flush_whole_async(cache);
	}
}

int cache_get_page(cache_t *cache, off_t offset, uintptr_t *page_ret) {
	uintptr_t page = cache_lookup_and_ref_page(cache, offset);
	int need_read = 0;
	int ret = 0;
	if (page == PAGE_INVALID) {
		int raced;
		page = cache_setup_page(cache, offset, &raced);
		if (page == PAGE_INVALID) return -ENOMEM;
		if (!raced) need_read = 1;
	}

	if (need_read) {
		// we need to load the page
		// cache_read_pages is going to consume a ref
		pmm_retain(page);
		pages_batch_t pages_batch;
		pages_batch_from_page(&pages_batch, page);
		ret = cache_read_pages(cache, &pages_batch);
	} else {
		// mark the page was accessed so the evicter know about it
		cache_mark_page_active(cache, page);
	}
	if (ret >= 0) {
		ret = cache_wait_page_ready(page);
	}
	if (ret < 0) {
		pmm_release_page(page);
		return ret;
	}
	*page_ret = page;
	return 0;
}

int cache_preload(cache_t *cache, off_t offset, size_t size) {
	if (!cache->ops || !cache->ops->read) return -EINVAL;

	uintptr_t start, end;
	cache_get_range(cache, offset, size, &start, &end);

	pages_batch_t pages_batch;
	pages_batch_init(&pages_batch);
	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
		uintptr_t page = cache_lookup_page(cache, addr);
		// fast path
		if (page != PAGE_INVALID) {
			// the page is already cached
			// there is nothing to load
			continue;
		}

		int raced;
		page = cache_setup_page(cache, addr, &raced);
		if (page == PAGE_INVALID) {
			if (!pages_batch_is_empty(&pages_batch)) {
				int ret = cache_read_pages(cache, &pages_batch);
				if (ret < 0) return ret;
			}
			return -ENOMEM;
		}
		if (raced) {
			pmm_release_page(page);
			continue;
		}

		if (pages_batch_is_full(&pages_batch)) {
			int ret = cache_read_pages(cache, &pages_batch);
			if (ret < 0) {
				pmm_release_page(page);
				return ret;
			}
			pages_batch_init(&pages_batch);
		}

		pages_batch_add(&pages_batch, page);
	}

	if (!pages_batch_is_empty(&pages_batch)) {
		return cache_read_pages(cache, &pages_batch);
	}
	return 0;
}

int cache_flush_async(cache_t *cache, off_t offset, size_t size) {
	if (!cache->ops || !cache->ops->write) return -EINVAL;

	uintptr_t start, end;
	cache_get_range(cache, offset, size, &start, &end);

	pages_batch_t batch;
	pages_batch_init(&batch);

	rcu_acquire_read(&cache->pages.rcu);
	cache_foreach_range(addr, page, cache, start, end) {
		if (!cache_clear_page_dirty(cache, page)) {
			continue;
		}

		pmm_retain(page);

		page_t *page_info = pmm_page_info(page);
		rcu_release_read(&cache->pages.rcu);
		cache_wait_page_no_io(page);
		while (atomic_fetch_or(&page_info->flags, PAGE_FLAG_WRITING) & PAGE_FLAG_WRITING) {
			// already writing
			// wait until write complete
			cache_wait_page_no_io(page);
		}
		pmm_acquire_page_lock(page);
		if (cached_page_is_evicted(page_info)) {
			atomic_fetch_and(&page_info->flags, ~PAGE_FLAG_WRITING);
			pmm_release_page_lock(page);
			rcu_acquire_read(&cache->pages.rcu);
			pmm_release_page(page);
			continue;
		}
		pmm_release_page_lock(page);

		if (pages_batch_is_full(&batch)) {
			int ret = cache_write_pages(cache, &batch);
			if (ret < 0) {
				pmm_release_page(page);
				return ret;
			}
			pages_batch_init(&batch);
		}

		pages_batch_add(&batch, page);
		rcu_acquire_read(&cache->pages.rcu);
	}
	rcu_release_read(&cache->pages.rcu);

	if (!pages_batch_is_empty(&batch)) {
		return cache_write_pages(cache, &batch);
	}
	return 0;
}

int cache_flush(cache_t *cache, off_t offset, size_t size) {
	int ret = cache_flush_async(cache, offset, size);
	if (ret < 0) return ret;

	// wait for each page to be fully written
	uintptr_t start, end;
	cache_get_range(cache, offset, size, &start, &end);
	cache_foreach_range(addr, page, cache, start, end) {
		(void)addr;
		ret = cache_wait_page_written(page);
		if (ret < 0) return ret;
	}
	return 0;
}

// mapping cache

static int cache_vmm_msync(vmm_seg_t *seg, uintptr_t start, uintptr_t end, int flags) {
	(void)flags;
	// never sync private mappings
	if (seg->flags & VMM_FLAG_PRIVATE) return 0;

	cache_t *cache = seg->private_data;

	for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
		uintptr_t page = mmu_virt2phys((void *)addr);
		if (page == PAGE_INVALID) continue;
		long mmu_flags = mmu_get_and_clear_flags(get_current_proc()->vmm_space.addrspace, addr, MMU_FLAG_DIRTY | MMU_FLAG_ACCESS);
		if (mmu_flags & MMU_FLAG_DIRTY) {
			cache_mark_page_dirty(cache, page);
		}
		if (mmu_flags & MMU_FLAG_ACCESS) {
			cache_mark_page_active(cache, page);
		}
	}

	if (flags & VMM_FLAG_SYNC) {
		return cache_flush(cache, seg->offset + start - seg->start, end - start);
	} else {
		return 0;
	}
}

static int cache_vmm_fault(vmm_seg_t *seg, uintptr_t addr, long prot) {
	if (!(prot & seg->prot)) return 0;

	if (mmu_virt2phys((void *)addr) != PAGE_INVALID) {
		// the page is already mapped it's not our job
		return 0;
	}

	cache_t *cache  = seg->private_data;
	uintptr_t vpage = PAGE_ALIGN_DOWN(addr);
	off_t offset    = vpage - seg->start + seg->offset;

	uintptr_t page;
	int ret = cache_get_page(cache, offset, &page);
	if (ret < 0) {
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
			uintptr_t new_page = pmm_dup_page(page);
			pmm_release_page(page);
			if (new_page == PAGE_INVALID) {
				signal_send_task(get_current_task(), SIGBUS);
				return 1;
			}
			page = new_page;
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
		uintptr_t page;
		ret = cache_get_page(cache, addr, &page);
		if (ret < 0) break;
		
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
		uintptr_t page;
		ret = cache_get_page(cache, addr, &page);
		if (ret < 0) break;
		
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
	mutex_acquire(&cache->mutex);
	if (size < cache->size) {
		uintptr_t start = PAGE_ALIGN_UP(size);
		uintptr_t end   = PAGE_ALIGN_UP(cache->size);
		cache_free_pages(cache, start, end - start);
	} else if (size > cache->size && cache->size % PAGE_SIZE != 0) {
		// we need to zero the last page
		rcu_acquire_read(&cache->pages.rcu);
		uintptr_t page = cache_lookup_page(cache, PAGE_ALIGN_DOWN(cache->size));
		if (page != PAGE_INVALID) {
			char *vaddr = mmu_phys2virt(page);
			size_t partial_size = cache->size % PAGE_SIZE;
			memset(vaddr + partial_size, 0, PAGE_SIZE - partial_size);
			cache_mark_page_dirty(cache, page);
		}
		rcu_release_read(&cache->pages.rcu);
	}
	cache->size = size;
	mutex_release(&cache->mutex);
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
