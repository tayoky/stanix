#ifndef KERNEL_CACHE_H
#define KERNEL_CACHE_H

#include <kernel/rwlock.h>
#include <kernel/xarray.h>

struct cache;
struct vmm_seg;
struct vfs_fd;

typedef struct cache_op {
	int (*read)(struct cache *cache, off_t offset, size_t count);
	int (*write)(struct cache *cache, off_t offset, size_t count);
	int (*ioctl)(struct cache *cache, long req, void *arg);
} cache_ops_t;

typedef struct cache {
	list_node_t node;
	list_node_t dirty_node; // protected by dirty_lock
	size_t dirty_count;     // protected by dirty_lock
	xarray_t pages;
	cache_ops_t *ops;
	size_t size;
} cache_t;

void init_cache(cache_t *cache);
void free_cache(cache_t *cache);
int cache_get_page(cache_t *cache, off_t offset, uintptr_t *page);
int cache_preload(cache_t *cache, off_t offset, size_t size);
int cache_flush_async(cache_t *cache, off_t offset, size_t size);
int cache_flush(cache_t *cache, off_t offset, size_t size);
int cache_flush_whole_async(cache_t *cache);
int cache_flush_whole(cache_t *cache);
int cache_mmap(cache_t *cache, off_t offset, struct vmm_seg *seg);
ssize_t cache_read(cache_t *cache, void *buffer, off_t offset, size_t size);
ssize_t cache_write(cache_t *cache, const void *buffer, off_t offset, size_t size);
int cache_truncate(cache_t *cache, size_t size);
int cache_open(cache_t *cache, struct vfs_fd *fd);
void cache_read_terminate(cache_t *cache, off_t offset, size_t size, int ret);
void cache_write_terminate(cache_t *cache, off_t offset, size_t size, int ret);
void cache_flush_all(void);

static inline uintptr_t cache_lookup_page(cache_t *cache, off_t offset) {
	uintptr_t page = (uintptr_t)xarray_get(&cache->pages, PAGE2PFN(offset));
	if (!page) return PAGE_INVALID;
	return page;
}

static inline uintptr_t cache_lookup_and_ref_page(cache_t *cache, off_t offset) {
	rcu_acquire_read(&cache->pages.rcu);
	uintptr_t page = (uintptr_t)xarray_get(&cache->pages, PAGE2PFN(offset));
	if (!page) {
		rcu_release_read(&cache->pages.rcu);
		return PAGE_INVALID;
	}
	rcu_release_read(&cache->pages.rcu);
	return pmm_retain(page);
}

static inline uintptr_t cache_get_next_page(cache_t *cache, off_t after, off_t *offset) {
	size_t index;
	uintptr_t page = (uintptr_t)xarray_next(&cache->pages, PAGE2PFN(after), &index);
	if (!page) return PAGE_INVALID;
	if (offset) *offset = PFN2PAGE(index);
	return page;
}


#define cache_foreach_range(addr, page, cache, start, end) \
	loop_var(size_t, index) \
	loop_var(uintptr_t, addr) \
	loop_var(uintptr_t, page) \
	for (page = (uintptr_t)xarray_first_from(&(cache)->pages, (start), &index), addr = PFN2PAGE(index); page && addr < (end); page = (uintptr_t)xarray_next(&(cache)->pages, index, &index), addr = PFN2PAGE(index))

#endif
