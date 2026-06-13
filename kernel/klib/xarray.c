#include <kernel/assert.h>
#include <kernel/slab.h>
#include <kernel/string.h>
#include <kernel/xarray.h>

static slab_cache_t xarray_nodes_slab;

static int xarray_entry_constructor(slab_cache_t *cache, void *data) {
	(void)cache;
	xarray_node_t *node = data;
	memset(node, 0, sizeof(xarray_node_t));
	return 0;
}

static inline uintptr_t xarray_entry_fetch(rcu_ptr_t *ptr) {
	return (uintptr_t)rcu_ptr_fetch(ptr);
}

static inline uintptr_t xarray_entry_store(rcu_ptr_t *ptr, uintptr_t entry) {
	return (uintptr_t)rcu_ptr_store(ptr, (void *)entry);
}

static inline void *xarray_entry_get_value(uintptr_t xarray_entry) {
	return (void *)(xarray_entry & ~0x1);
}

static inline xarray_node_t *xarray_entry_get_node(uintptr_t xarray_entry) {
	return (xarray_node_t *)xarray_entry;
}

static inline int xarray_entry_is_value(uintptr_t xarray_entry) {
	return xarray_entry & 0x1;
}

static inline int xarray_entry_is_node(uintptr_t xarray_entry) {
	return !xarray_entry_is_value(xarray_entry);
}

static inline uintptr_t xarray_entry_from_value(void *value) {
	return ((uintptr_t)value) | 0x1;
}

static inline uintptr_t xarray_entry_from_node(xarray_node_t *node) {
	return (uintptr_t)node;
}

void xarray_init(xarray_t *xarray) {
	memset(xarray, 0, sizeof(xarray_t));
}

static void xarray_entry_destroy(uintptr_t entry) {
	if (!entry) return;
	if (xarray_entry_is_value(entry)) return;

	xarray_node_t *node = xarray_entry_get_node(entry);
	for (size_t i = 0; i < XARRAY_ENTRIES_PER_NODE; i++) {
		// no need for xarray_entry_fetch since we are destroying
		// so nobody can access it and there are no risk of race condtions
		xarray_entry_destroy(node->entries[i]);
	}
	slab_free(node);
}

void xarray_destroy(xarray_t *xarray) {
	rcu_acquire_write(&xarray->rcu);
	xarray_entry_destroy(xarray_entry_fetch(&xarray->rcu.ptr));
}

static inline size_t xarray_node_get_local_index(xarray_node_t *node, size_t index) {
	return (index >> node->shift) & XARRAY_MASK;
}

static inline rcu_ptr_t *xarray_node_get_entry(xarray_node_t *node, size_t index) {
	size_t local_index = xarray_node_get_local_index(node, index);
	return &node->entries[local_index];
}

static inline int xarray_is_in_bound(uintptr_t entry, size_t index) {
	size_t size;
	if (!entry) {
		size = 1U;
	} else if (xarray_entry_is_value(entry)) {
		size = 1U;
	} else {
		xarray_node_t *node = xarray_entry_get_node(entry);
		size                = 1UL << (node->shift + XARRAY_SHIFT_BITS);
	}
	if (index >= size) {
		return 0;
	}
	return 1;
}

static void *xarray_raw_get(xarray_t *xarray, size_t index) {
	uintptr_t current_entry = xarray_entry_fetch(&xarray->rcu.ptr);

	// bound check
	if (!xarray_is_in_bound(current_entry, index)) return NULL;

	while (current_entry) {
		if (xarray_entry_is_value(current_entry)) {
			return xarray_entry_get_value(current_entry);
		} else {
			xarray_node_t *node = xarray_entry_get_node(current_entry);
			size_t local_index  = xarray_node_get_local_index(node, index);
			current_entry       = xarray_entry_fetch(&node->entries[local_index]);
		}
	}
	return NULL;
}

void *xarray_get(xarray_t *xarray, size_t index) {
	rcu_acquire_read(&xarray->rcu);
	void *ret = xarray_raw_get(xarray, index);
	rcu_release_read(&xarray->rcu);
	return ret;
}

static void *xarray_raw_set(xarray_t *xarray, size_t index, void *value) {
	rcu_ptr_t *current_entry = &xarray->rcu.ptr;

	uintptr_t current_entry_value = xarray_entry_fetch(current_entry);
	if (!xarray_is_in_bound(current_entry_value, index)) {
		if (!value) return NULL;
		// we need to grow the array
		// but how many level ?
		size_t current_shift;
		int previous_is_full = 0;
		if (!current_entry_value) {
			current_shift = 0;
		} else if (xarray_entry_is_value(current_entry_value)) {
			current_shift    = 0;
			previous_is_full = 1;
		} else {
			xarray_node_t *node = xarray_entry_get_node(current_entry_value);
			current_shift       = node->shift + XARRAY_SHIFT_BITS;
			previous_is_full    = node->full_mask == UINT64_MAX;
		}

		size_t target_shift = 0;
		while (index >= (1UL << target_shift)) {
			target_shift += XARRAY_SHIFT_BITS;
		}

		// we have the target shift and the current shift
		// we can find how many level we need to add
		kassert(current_shift < target_shift);

		while (current_shift < target_shift) {
			xarray_node_t *node = slab_alloc(&xarray_nodes_slab);
			node->shift         = current_shift;
			node->entries[0]    = current_entry_value;
			node->full_mask     = previous_is_full ? 0x1 : 0x0;
			current_entry_value = xarray_entry_from_node(node);
			current_shift += XARRAY_SHIFT_BITS;
			previous_is_full = 0;
		}
		xarray_entry_store(&xarray->rcu.ptr, current_entry_value);
	}

	// keep track of the nodes we have gone through to set the mask on them
	xarray_node_t *parents[12];
	size_t parents_count = 0;

	size_t current_shift = 0;
	while (current_entry_value) {
		if (xarray_entry_is_value(current_entry_value)) {
			uintptr_t old = xarray_entry_store(current_entry, xarray_entry_from_value(value));
			if (!value) return xarray_entry_get_value(old);
			// backtrack to set mask
			for (size_t i = parents_count; i > 0;) {
				i--;
				// TODO : we might even do some freeing here
				size_t local_index = xarray_node_get_local_index(parents[i], index);
				parents[i]->full_mask |= (1ULL << local_index);
				if (parents[i]->full_mask != UINT64_MAX) {
					// the mask is not full
					// no need to report on parent
					break;
				}
			}
			return xarray_entry_get_value(old);
		} else {
			xarray_node_t *node = xarray_entry_get_node(current_entry_value);
			if (value) {
				// only need to keep track of the ndoe we go through when value is non NULL
				parents[parents_count++] = node;
			} else {
				size_t local_index = xarray_node_get_local_index(node, index);
				node->full_mask &= ~(1ULL << local_index);
			}
			current_shift       = node->shift;
			current_entry       = xarray_node_get_entry(node, index);
			current_entry_value = xarray_entry_fetch(current_entry);
		}
	}

	if (!value) return NULL;
	kassert((current_shift % XARRAY_SHIFT_BITS) == 0);
	while (current_shift >= XARRAY_SHIFT_BITS) {
		current_shift -= XARRAY_SHIFT_BITS;
		xarray_node_t *node = slab_alloc(&xarray_nodes_slab);
		node->shift         = current_shift;
		size_t local_index  = xarray_node_get_local_index(node, index);
		node->full_mask |= (1ULL << local_index);
		xarray_entry_store(current_entry, xarray_entry_from_node(node));
		current_entry = xarray_node_get_entry(node, index);
	}
	xarray_entry_store(current_entry, xarray_entry_from_value(value));
	return NULL;
}

void *xarray_set(xarray_t *xarray, size_t index, void *value) {
	kassert((((uintptr_t)value) & 0x1) == 0);
	rcu_acquire_write(&xarray->rcu);
	void *ret = xarray_raw_set(xarray, index, value);
	rcu_release_write(&xarray->rcu);
	return ret;
}

// return 1 if found
static int xarray_entry_allocate(rcu_ptr_t *entry, size_t min, size_t parent_shift, size_t *index, int *full, void *value) {
	uintptr_t entry_value = xarray_entry_fetch(entry);
	// normally we can't get a value
	if (!entry_value) {
		if (full) *full = 1;
		while (parent_shift >= XARRAY_SHIFT_BITS) {
			parent_shift -= XARRAY_SHIFT_BITS;

			// we need to allocate new substrees so it's not full
			if (full) *full = 0;
			// we need to allocate subtrees
			xarray_node_t *node = slab_alloc(&xarray_nodes_slab);
			node->shift         = parent_shift;
			xarray_entry_store(entry, xarray_entry_from_node(node));
			entry = xarray_node_get_entry(node, min);
		}
		if (index) *index = 0;
		xarray_entry_store(entry, xarray_entry_from_value(value));
		return 1;
	} else if (xarray_entry_is_value(entry_value)) {
		return 0;
	} else {
		// we need to find a non full entry
		xarray_node_t *node = xarray_entry_get_node(entry_value);
		if (node->full_mask == UINT64_MAX) return 0;

		size_t start = xarray_node_get_local_index(node, min);
		for (size_t i = start; i < XARRAY_ENTRIES_PER_NODE; i++) {
			if (node->full_mask & (1UL << i)) continue;
			int child_is_full;
			if (xarray_entry_allocate(&node->entries[i], i == start ? min : 0, node->shift, index, &child_is_full, value)) {
				if (index) *index += i << node->shift;
				if (child_is_full) node->full_mask |= (1UL << i);
				if (full) *full = (node->full_mask == UINT64_MAX);
				return 1;
			}
		}
	}
	return 0;
}

static size_t xarray_raw_allocate_from(xarray_t *xarray, size_t start, void *value) {
	rcu_ptr_t *entry = &xarray->rcu.ptr;
	uintptr_t entry_value = xarray_entry_fetch(entry);

	// get rid of the case where start is out of bound
	if (!xarray_is_in_bound(entry_value, start)) {
		xarray_raw_set(xarray, start, value);
		return start;
	}

	size_t index;
	int child_is_full;
	if (!xarray_entry_allocate(entry, start, 0, &index, &child_is_full, value)) {
		// reroot to make place
		xarray_node_t *new_root = slab_alloc(&xarray_nodes_slab);
		new_root->entries[0]    = entry_value;
		new_root->full_mask     = child_is_full ? 0x1 : 0x0;
		if (xarray_entry_is_value(entry_value)) {
			new_root->shift = 0;
		} else {
			xarray_node_t *old_root = xarray_entry_get_node(entry_value);
			new_root->shift         = old_root->shift + XARRAY_SHIFT_BITS;
		}
		entry_value = xarray_entry_from_node(new_root);
		xarray_entry_store(entry, entry_value);
		xarray_entry_allocate(&new_root->entries[1], 0, new_root->shift, &index, NULL, value);
		index += (1UL << new_root->shift);
	}
	return index;
}

size_t xarray_allocate_from(xarray_t *xarray, size_t start, void *value) {
	kassert((((uintptr_t)value) & 0x1) == 0);
	rcu_acquire_write(&xarray->rcu);
	size_t ret = xarray_raw_allocate_from(xarray, start, value);
	rcu_release_write(&xarray->rcu);
	return ret;
}

void init_xarray(void) {
	slab_init(&xarray_nodes_slab, sizeof(xarray_node_t), "xarray-nodes");
	xarray_nodes_slab.constructor = xarray_entry_constructor;
}

/**
 * @brief find a non null entry
 * @param entry the entry to search in
 * @param min the index to start the search at
 * @param index where to store the index of the found entry
 */
static void *xarray_entry_find_non_null(uintptr_t entry, size_t min, size_t *index) {
	if (!entry) {
		return NULL;
	} else if (xarray_entry_is_value(entry)) {
		if (index) *index = 0;
		return xarray_entry_get_value(entry);
	} else {
		// we need to find a non NULL entry
		xarray_node_t *node = xarray_entry_get_node(entry);
		size_t start        = xarray_node_get_local_index(node, min);
		for (size_t i = start; i < XARRAY_ENTRIES_PER_NODE; i++) {
			uintptr_t current_entry = xarray_entry_fetch(&node->entries[i]);
			void *ret               = xarray_entry_find_non_null(current_entry, i == start ? min : 0, index);
			if (ret) {
				if (index) *index += i << node->shift;
				return ret;
			}
		}
	}
	return NULL;
}

static void *xarray_raw_first(xarray_t *xarray, size_t *index) {
	uintptr_t entry = xarray_entry_fetch(&xarray->rcu.ptr);
	return xarray_entry_find_non_null(entry, 0, index);
}

void *xarray_first(xarray_t *xarray, size_t *index) {
	rcu_acquire_read(&xarray->rcu);
	void *ret = xarray_raw_first(xarray, index);
	rcu_release_read(&xarray->rcu);
	return ret;
}

static void *xarray_raw_next(xarray_t *xarray, size_t after, size_t *index) {
	uintptr_t entry = xarray_entry_fetch(&xarray->rcu.ptr);
	if (!xarray_is_in_bound(entry, after + 1)) return NULL;
	return xarray_entry_find_non_null(entry, after + 1, index);
}

void *xarray_next(xarray_t *xarray, size_t after, size_t *index) {
	rcu_acquire_read(&xarray->rcu);
	void *ret = xarray_raw_next(xarray, after, index);
	rcu_release_read(&xarray->rcu);
	return ret;
}

static void xarray_debug_depth(size_t depth) {
	kdebugf("% *.s", (int)depth * 2, "");
}

static void xarray_debug_entry(uintptr_t entry, size_t depth) {
	xarray_debug_depth(depth);

	if (!entry) {
		kprintf("NULL entry\n");
	} else if (xarray_entry_is_value(entry)) {
		kprintf("value %p\n", xarray_entry_get_value(entry));
	} else {
		xarray_node_t *node = xarray_entry_get_node(entry);
		// NOTE : technlicly note safe safe mask isen't rcu
		// but it's just for debug so ...
		kprintf("xarray node of shift %ld and mask %lx\n", node->shift, node->full_mask);
		for (size_t i = 0; i < XARRAY_ENTRIES_PER_NODE; i++) {
			xarray_debug_depth(depth);
			kprintf("at index %zu\n", i);
			uintptr_t current_entry = xarray_entry_fetch(&node->entries[i]);
			xarray_debug_entry(current_entry, depth + 1);
		}
	}
}

static void xarray_raw_debug(xarray_t *xarray) {
	uintptr_t entry = xarray_entry_fetch(&xarray->rcu.ptr);
	xarray_debug_entry(entry, 0);
}

void xarray_debug(xarray_t *xarray) {
	rcu_acquire_read(&xarray->rcu);
	xarray_raw_debug(xarray);
	rcu_release_read(&xarray->rcu);
}
