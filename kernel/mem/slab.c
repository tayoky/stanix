#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/string.h>
#include <kernel/slab.h>
#include <kernel/pmm.h>
#include <kernel/mmu.h>

static list_t slabs_list;

static void slab_calculate_order(slab_cache_t *slab_cache) {
	// for small object keep small order
	if (slab_cache->size < 1024) {
		slab_cache->order = ORDER_SIZE1;
		return;
	}

	size_t best_waste = SIZE_MAX;
	int best_order = ORDER_SIZE64;
	for (int order=0; order<ORDERS_COUNT; order++) {
		size_t usable_size = ORDER2COUNT(order) * PAGE_SIZE - sizeof(slab_t);
		size_t objects_per_slab = usable_size / slab_cache->size;
		if (objects_per_slab == 0) continue;
		size_t used_size = objects_per_slab * slab_cache->size;
		size_t wasted_size = usable_size - used_size;

		// big order need really smaller wasted to be choiced
		// so we give a penalty based on order
		size_t penalty = 256 * (ORDER2COUNT(order) - 1);
		if (wasted_size + penalty <= best_waste) {
			best_waste = wasted_size;
			best_order = order;
		}
	}
	slab_cache->order = best_order;
}

int slab_init(slab_cache_t *slab_cache, size_t size, const char *name) {
	if (size > PAGE_SIZE - sizeof(slab_t)) return -EINVAL;
	kdebugf("init slab '%s'\n", name);
	memset(slab_cache, 0, sizeof(slab_cache_t));
	slab_cache->name = name;
	slab_cache->size = size;
	slab_calculate_order(slab_cache);
	list_append(&slabs_list, &slab_cache->node);
	return 0;
}

void slab_destroy(slab_cache_t *slab_cache) {
	list_remove(&slabs_list, &slab_cache->node);
}

list_t *slab_get_list(void) {
	return &slabs_list;
}

static slab_t *new_slab(slab_cache_t *slab_cache) {
	uintptr_t page = pmm_allocate_pages(slab_cache->order);
	if (page == PAGE_INVALID) return NULL;

	slab_t *slab = mmu_phys2virt(page);
	memset(slab, 0, sizeof(slab_t));
	slab->cache = slab_cache;
	slab->state = SLAB_FREE;
	for (int i=0; i<ORDER2COUNT(slab_cache); i++) {
		pmm_page_info(page + i * PAGE_SIZE)->private = slab;
	}

	// init the free list
	size_t objects_count = (ORDER2COUNT(slab_cache->order) * PAGE_SIZE - sizeof(slab_t)) / slab_cache->size;
	for (size_t i=0; i<objects_count; i++) {
		slab_free_node_t *current = (slab_free_node_t*)((uintptr_t)slab + sizeof(slab_t) + i * slab_cache->size);
		if (i == objects_count - 1) {
			// this is the last element
			current->next = NULL;
		} else {
			slab_free_node_t *next = (slab_free_node_t*)((uintptr_t)slab + sizeof(slab_t) + (i+1) * slab_cache->size);
			current->next = next;
		}
	}
	slab->free = (slab_free_node_t*)((uintptr_t)slab + sizeof(slab_t));
	return slab;
}

static void *slab_evict(slab_cache_t *slab_cache) {
	if (!slab_cache->evict) return NULL;
	void *data = slab_cache->evict(slab_cache);
	if (!data) return NULL;
	if (slab_cache->destructor) slab_cache->destructor(slab_cache, data);
	return data;
}

static void free_slab(slab_t *slab) {
	uintptr_t pages = mmu_virt2phys(slab);
	pmm_set_free_pages(pages, slab->cache->order);
}

void *slab_alloc(slab_cache_t *slab_cache) {
	spinlock_acquire(&slab_cache->lock);
	slab_t *slab = container_of(slab_cache->partial.first_node, slab_t, node);
	if (!slab) {
		// we have no partial slab
		// take a free slab and move it into partial
		slab = container_of(slab_cache->free.first_node, slab_t, node);
		if (slab) {
			list_remove(&slab_cache->free, &slab->node);
		} else {
			// we need to create a new slab
			slab = new_slab(slab_cache);
			if (!slab) {
				// FIXME : maybee we should release later
				spinlock_release(&slab_cache->lock);
				// mayee we can evict ?
				void *data = slab_evict(slab_cache);
				if (!data) return NULL;
				if (slab_cache->constructor) {
					slab_cache->constructor(slab_cache, data);
				}
				return data;
			}
		}

		// move the slab in partial list
		slab->state = SLAB_PARTIAL;
		list_add_after(&slab_cache->partial, NULL, &slab->node);
	}

	slab_free_node_t *node = slab->free;
	slab->free = node->next;
	slab->allocated_count++;

	if (!slab->free) {
		// the slab is full
		slab->state = SLAB_FULL;
		list_remove(&slab_cache->partial, &slab->node);
		list_append(&slab_cache->full, &slab->node);
	}

	spinlock_release(&slab_cache->lock);
	if (slab_cache->constructor) {
		slab_cache->constructor(slab_cache, node);
	}
	return node;
}

void slab_free(void *ptr) {
	uintptr_t page = PAGE_ALIGN_DOWN(mmu_virt2phys(ptr));
	slab_t *slab = pmm_page_info(page)->private;
	slab_cache_t *slab_cache = slab->cache;

	if (slab_cache->destructor) {
		slab_cache->destructor(slab_cache, ptr);
	}

	spinlock_acquire(&slab_cache->lock);

	// add the object to the list of free slot
	slab_free_node_t *node = ptr;
	node->next = slab->free;
	slab->free = node;

	slab->allocated_count--;
	if (slab->allocated_count == 0) {
		slab->state = SLAB_FREE;
		list_remove(&slab_cache->partial, &slab->node);
		// TODO : maybee we shouldn't free now ???
		free_slab(slab);
	} else if (slab->state == SLAB_FULL) {
		// it's not full anymore
		slab->state = SLAB_PARTIAL;
		list_remove(&slab_cache->full, &slab->node);
		list_add_after(&slab_cache->partial, NULL, &slab->node);
	}

	spinlock_release(&slab_cache->lock);
}
