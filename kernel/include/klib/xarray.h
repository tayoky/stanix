#ifndef KERNEL_XARRAY_H
#define KERNEL_XARRAY_H

#include <kernel/rcu.h>

#define XARRAY_SHIFT_BITS       6
#define XARRAY_ENTRIES_PER_NODE (1 << XARRAY_SHIFT_BITS)
#define XARRAY_MASK             ((1 << XARRAY_SHIFT_BITS) - 1)

typedef struct xarray_node {
	size_t shift;       // hold the shift of the current mask
	uint64_t full_mask; // bitmap of is each entry full ?
	rcu_ptr_t entries[XARRAY_ENTRIES_PER_NODE];
} xarray_node_t;

typedef struct xarray {
	rcu_t rcu;
} xarray_t;

/**
 * @brief initalize the xarray subsystem
 */
void init_xarray(void);

/**
 * @brief initalize a xarray
 * @param xarray the xarray to initalize
 */
void xarray_init(xarray_t *xarray);

/**
 * @brief destroy a xarray
 * @param xarray the xarray to destroy
 */
void xarray_destroy(xarray_t *xarray);

/**
 * @brief get a value of an xarray by index
 * @param xarray the xarray to fetch the value from
 * @param index the index at wich to find the value
 * @return the value at the specfied index
 */
void *xarray_get(xarray_t *xarray, size_t index);

/**
 * @brief set a value in a xarray by index
 * @param xarray the xarray in which to set the value
 * @param index the index of the value to set
 * @param value the new value, must be 2 bytes aligned
 */
void xarray_set(xarray_t *xarray, size_t index, void *value);

/**
 * @brief allocate an index inside a xarray, the allocated index will be the index of the first NULL value starting from start
 * @param xarray the xarray in which to allocate an index
 * @param start the index at which to start search for a NULL entry
 * @param value the value to set to the allocted index
 * @return the allocated index
 */
size_t xarray_allocate_from(xarray_t *xarray, size_t start, void *value);

/**
 * @brief allocate an index inside a xarray, the allocated index will be the index of the first NULL value
 * @param xarray the xarray in which to allocate an index
 * @param value the value to set to the allocted index
 * @return the allocated index
 */
static inline size_t xarray_allocate(xarray_t *xarray, void *value) {
	xarray_allocate_from(xarray, 0, value);
}

/**
 * @brief clear a value in a xarray by index
 * @param xarray the xarray in which to clear the value
 * @param index the index of the value to clear
 */
static inline void xarray_clear(xarray_t *xarray, size_t index) {
	return xarray_set(xarray, index, NULL);
}

/**
 * @brief get the first non NULL value in a xarray
 * @param xarray the xarray in which to find the first value of
 * @param index where to store the index of the first value
 * @return the first non NULL value
 */
void *xarray_first(xarray_t *xarray, size_t *index);

/**
 * @brief get the first non NULL value in a xarray after a speficied index
 * @param xarray the xarray in which to find the first value of
 * @param after find a non NULL array after this index
 * @param index where to store the index of the first value
 * @return the first non NULL value after a specified index
 */
void *xarray_next(xarray_t *xarray, size_t after, size_t *index);

/**
 * @brief print debug info about an xarray
 * @param xarray the xarray to debug
 */
void xarray_debug(xarray_t *xarray);

/**
 * @brief iterate over each non NULL value in a xarray
 * @param index the name of the variable to hold the index
 * @param value the name of the variable to hold the value
 * @param xarray the xarray to iterate on
 */
#define xarray_foreach(index, value, xarray) \
	for (size_t index, _1 = 1; _1; _1 = 0) \
		for (void *value, *_2 = (void *)1; _2; _2 = NULL) \
			for (value = xarray_first(xarray, &index); value; value = xarray_next(xarray, index, &index))

#endif
