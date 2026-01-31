#include <kernel/hashmap.h>
#include <kernel/string.h>
#include <kernel/assert.h>
#include <kernel/kernel.h>
#include <kernel/cache.h>
#include <kernel/cond.h>
#include <kernel/vmm.h>
#include <kernel/vfs.h>
#include <kernel/pmm.h>

void init_cache(cache_t *cache) {
	memset(cache, 0, sizeof(cache_t));
	init_hashmap(&cache->pages, 256);
}

static void cleanup_page(void *element, long off, void *arg) {
	uintptr_t page = (uintptr_t)element;
	cache_t *cache = arg;
	
	cache->ops->write(cache, off, PAGE_SIZE, NULL, NULL);
	
	pmm_free_page(page);
}

void free_cache(cache_t *cache) {
	hashmap_foreach(&cache->pages, cleanup_page, cache);
	free_hashmap(&cache->pages);
}

static void set_condition(cache_t *cache, void *arg) {
	(void)cache;
	cond_t *cond = arg;
	cond_set(cond, 1);
}

int cache_cache_async(cache_t *cache, off_t offset, size_t size, cache_callback_t callback, void *arg) {
	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end  = PAGE_ALIGN_UP(offset + size);

	if (!cache->ops || !cache->ops->read) return -EINVAL;
	
	for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		if (page != PAGE_INVALID) {
			// the page is already cached
			// there is nothing to cache
			continue;
		}
		page = pmm_allocate_page();
		// FIXME : make this safer
		if (page == PAGE_INVALID) return -ENOMEM;
		hashmap_add(&cache->pages, addr, (void*)page);
	}
	int ret = cache->ops->read(cache, start, end - start, callback, arg);
	
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
	cond_t condition;
	init_cond(&condition);

	cond_set(&condition, 0);
	int ret = cache_cache_async(cache, offset, size, set_condition, &condition);
	if (ret < 0) return ret;
	return cond_wait_interruptible(&condition, 1);	
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
	
	return cache->ops->write(cache, start, end - start, callback, arg);
}

int cache_flush(cache_t *cache, off_t offset, size_t size) {
	cond_t condition;
	init_cond(&condition);

	cond_set(&condition, 0);
	int ret = cache_flush_async(cache, offset, size, set_condition, &condition);
	if (ret < 0) return ret;
	return cond_wait_interruptible(&condition, 1);	
} 

int cache_mmap(cache_t *cache, off_t offset, vmm_seg_t *seg) {
	if (offset % PAGE_SIZE) return -EINVAL;
	cache_cache(cache, offset, VMM_SIZE(seg));
	
	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end  = PAGE_ALIGN_UP(offset + VMM_SIZE(seg));
	uintptr_t vaddr = seg->start;

	for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		kassert(page != PAGE_INVALID);
		pmm_retain(page);
		mmu_map_page(get_current_proc()->addrspace, page, vaddr, seg->prot);
		vaddr += PAGE_SIZE;
	}
	return 0;
}

ssize_t cache_read(cache_t *cache, void *buffer, off_t offset, size_t size) {
	if ((size_t)offset >= cache->size) return 0;
	if (offset + size > cache->size) size = cache->size - offset;
	// TODO : lock around this
	int ret = cache_cache(cache, offset, size);
	if (ret < 0) return ret;

	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end  = PAGE_ALIGN_UP(offset + size);

	// TODO : do the read here
	rwlock_acquire_read(&cache->lock);
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
		memcpy(buf, (void*)(kernel->hhdm + phys + page_start), page_end - page_start);
		buf += page_end - page_start;
	}
	rwlock_release_read(&cache->lock);
	return size;
}

ssize_t cache_write(cache_t *cache, const void *buffer, off_t offset, size_t size) {
	cache_cache(cache, offset, size);
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

static vfs_ops_t cache_ops = {
	.read  = cache_ops_read,
	.write = cache_ops_write,
	.ioctl = cache_ops_ioctl,
};

int cache_open(cache_t *cache, vfs_fd_t *fd) {
	fd->private = cache;
	fd->ops = &cache_ops;
	return 0;
}