#include <kernel/slab.h>
#include <kernel/string.h>
#include <kernel/xarray.h>

static slab_cache_t xarray_nodes_slab;

static inline uintptr_t xarray_entry_fetch(rcu_ptr_t *ptr) {
	return (uintptr_t)rcu_ptr_fetch(ptr);
}

static inline uintptr_t xarray_entry_store(rcu_ptr_t *ptr, uintptr_t entry) {
	return (uintptr_t)rcu_ptr_store(ptr, (void*)entry);
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

static inline uintptr_t xarray_entry_from_value(void *value) {
	return ((uintptr_t)value) & 0x1;
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

static inline void *xarray_raw_get(xarray_t *xarray, size_t index) {
	uintptr_t current_entry = xarray_entry_fetch(&xarray->rcu.ptr);
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

static void xarray_raw_set(xarray_t *xarray, size_t index, void *value) {
	rcu_ptr_t *current_entry = &xarray->rcu.ptr;
	while (*current_entry) {
		uintptr_t current_entry_value = xarray_entry_fetch(current_entry);
		if (xarray_entry_is_value(current_entry_value)) {
			xarray_entry_store(current_entry, xarray_entry_from_value(value));
		} else {
			xarray_node_t *node = xarray_entry_get_node(current_entry_value);
			size_t local_index  = xarray_node_get_local_index(node, index);
			current_entry       = &node->entries[local_index];
		}
	}
	// TODO : we need to allocate
}

void xarray_set(xarray_t *xarray, size_t index, void *value) {
	rcu_acquire_write(&xarray->rcu);
	xarray_raw_set(xarray, index, value);
	rcu_release_write(&xarray->rcu);
}
