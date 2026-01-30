#include <kernel/hashmap.h>
#include <kernel/string.h>
#include <kernel/assert.h>
#include <kernel/kernel.h>
#include <kernel/cache.h>
#include <kernel/vmm.h>
#include <kernel/pmm.h>

void init_cache(cache_t *cache) {
	memset(cache, 0, sizeof(cache_t));
	init_hashmap(&cache->pages, 256);
}

static void cleanup_page(void *element, long off, void *arg) {
	uintptr_t page = (uintptr_t)element;
	cache_t *cache = arg;
	
	if (cache->ops && cache->ops->write) {
		cache->ops->write(cache, (void*)(kernel->hhdm + page), off, PAGE_SIZE);
	}
	
	pmm_free_page(page);
}

void free_cache(cache_t *cache) {
	hashmap_foreach(&cache->pages, cleanup_page, cache);
	free_hashmap(&cache->pages);
}

int cache_cache(cache_t *cache, off_t offset, size_t size) {
	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end  = PAGE_ALIGN_UP(offset + size);

	if (!cache->ops || !cache->ops->read) return -EINVAL;
	
	for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, offset);
		if (page != PAGE_INVALID) {
			// the page is already cached
			// there is nothing to cache
			continue;
		}
		page = pmm_allocate_page();
		if (page == PAGE_INVALID) return -ENOMEM;
		ssize_t ret = cache->ops->read(cache, (void*)(kernel->hhdm + page), addr, PAGE_SIZE);
		if (ret < 0) {
			pmm_free_page(page);
			return (int)ret;
		}
		hashmap_add(&cache->pages, addr, (void*)page);
	}
	return ;
}

int cache_uncache(cache_t *cache, off_t offset, size_t size) {
	int ret = cache_flush(cache, offset, size);
	if (ret < 0) return ret;

	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end  = PAGE_ALIGN_UP(offset + size);
	
	for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, offset);
		if (page == PAGE_INVALID) {
			// the page is not cached
			// there is nothing to uncache
			continue;
		}
		pmm_free_page(page);
	}
	return 0;
}

int cache_flush(cache_t *cache, off_t offset, size_t size) {
	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end  = PAGE_ALIGN_UP(offset + size);

	if (!cache->ops || !cache->ops->write) return -EINVAL;
	
	for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, offset);
		if (page == PAGE_INVALID) {
			// the page is not cached
			// there is nothing to flush
			continue;
		}
		ssize_t ret = cache->ops->write(cache, (void*)(kernel->hhdm + page), addr, PAGE_SIZE);
		if (ret < 0) return (int)ret;
	}
	return 0;
}

int cache_mmap(cache_t *cache, off_t offset, vmm_seg_t *seg) {
	if (offset % PAGE_SIZE) return -EINVAL;
	cache_cache(cache, offset, VMM_SIZE(seg));
	
	uintptr_t start = PAGE_ALIGN_DOWN(offset);
	uintptr_t end  = PAGE_ALIGN_UP(offset + VMM_SIZE(seg));
	uintptr_t vaddr = seg->start;

	for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, offset);
		kassert(page != PAGE_INVALID);
		pmm_retain(page);
		mmu_map_page(get_current_proc()->addrspace, page, vaddr, seg->prot);
		vaddr += PAGE_SIZE;
	}
	return 0;
}

ssize_t cache_read(cache_t *cache, void *buffer, off_t offset, size_t size) {
	cache_cache(cache, offset, size);
}

ssize_t cache_write(cache_t *cache, const void *buffer, off_t offset, size_t size) {
	cache_cache(cache, offset, size);
}