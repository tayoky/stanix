#ifndef _KERNEL_CACHE_H
#define _KERNEL_CACHE_H

#include <kernel/rwlock.h>
#include <kernel/hashmap.h>

struct cache;
struct vmm_seg;
struct vfs_fd;

typedef void (*cache_callback_t)(struct cache *cache, void *arg);

typedef struct cache_op {
	int (*read)(struct cache *cache, off_t offset, size_t count);
	int (*write)(struct cache *cache, off_t offset, size_t count, cache_callback_t callback, void *arg);
	int (*ioctl)(struct cache *cache, long req, void *arg);
} cache_ops_t;

typedef struct cache {
	rwlock_t lock;
	hashmap_t pages;
	cache_ops_t *ops;
	size_t size;
} cache_t;


void init_cache(cache_t *cache);
void free_cache(cache_t *cache);
int cache_cache_async(cache_t *cache, off_t offset, size_t size);
int cache_uncache_async(cache_t *cache, off_t offset, size_t size, cache_callback_t callback, void *arg);
int cache_flush_async(cache_t *cache, off_t offset, size_t size, cache_callback_t callback, void *arg);
int cache_cache(cache_t *cache, off_t offset, size_t size);
int cache_uncache(cache_t *cache, off_t offset, size_t size);
int cache_flush(cache_t *cache, off_t offset, size_t size);
int cache_mmap(cache_t *cache, off_t offset, struct vmm_seg *seg);
ssize_t cache_read(cache_t *cache, void *buffer, off_t offset, size_t size);
ssize_t cache_write(cache_t *cache, const void *buffer, off_t offset, size_t size);
int cache_truncate(cache_t *cache, size_t size);
int cache_open(cache_t *cache, struct vfs_fd *fd);
void cache_read_terminate(cache_t *cache, off_t offset, size_t size);
void cache_write_terminate(cache_t *cache, off_t offset, size_t size, cache_callback_t callback, void *arg);

static inline uintptr_t cache_get_page(cache_t *cache, off_t offset) {
	uintptr_t page = (uintptr_t)hashmap_get(&cache->pages, offset);
	if (!page) return PAGE_INVALID;
	return page;
}

static inline void cache_call_callback(cache_t *cache, cache_callback_t callback, void *arg) {
	if (!callback) return;
	callback(cache, arg);
}

#endif
