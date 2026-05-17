#ifndef KERNEL_XARRAY_H
#define KERNEL_XARRAY_H

#include <kernel/rcu.h>

#define XARRAY_SHIFT_BITS       6
#define XARRAY_ENTRIES_PER_NODE (1 << XARRAY_SHIFT_BITS)
#define XARRAY_MASK ((1 << XARRAY_SHIFT_BITS) - 1)

typedef struct xarray_node {
	size_t shift;
	rcu_ptr_t entries[XARRAY_ENTRIES_PER_NODE];
} xarray_node_t;

typedef struct xarray {
	rcu_t rcu;
} xarray_t;

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
 * @param index the indedx of the valeu to set
 * @param value the new value
 */
void xarray_set(xarray_t *xarray, size_t index, void *value);

// TODO
#define xarray_foreach(xarray, index, value)

#endif
