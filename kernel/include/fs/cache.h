#ifndef _KERNEL_CACHE_H
#define _KERNEL_CACHE_H

#include <kernel/rwlock.h>
#include <kernel/hashmap.h>

struct cache;
struct vmm_seg;

typedef struct cache_op {
	ssize_t (*read)(struct cache *cache, void *buffer, off_t offset, size_t count);
	ssize_t (*write)(struct cache *cache, void *buffer, off_t offset, size_t count);
} cache_ops_t;

typedef struct cache {
	rwlock_t lock;
	cache_ops_t *ops;
	hashmap_t pages;
} cache_t;

void init_cache(cache_t *cache);
void free_cache(cache_t *cache);
int cache_cache(cache_t *cache, off_t offset, size_t size);
int cache_uncache(cache_t *cache, off_t offset, size_t size);
int cache_flush(cache_t *cache, off_t offset, size_t size);
int cache_mmap(cache_t *cache, off_t offset, struct vmm_seg *seg);
ssize_t cache_read(cache_t *cache, void *buffer, off_t offset, size_t size);
ssize_t cache_write(cache_t *cache, const void *buffer, off_t offset, size_t size);

static inline uintptr_t cache_get_page(cache_t *cache, off_t offset) {
	uintptr_t page = (uintptr_t)hashmap_get(&cache->pages, offset);
	if (!page) return PAGE_INVALID;
	return page;
}

#endif
