#ifndef KERNEL_KHEAP_H
#define KERNEL_KHEAP_H

#include <kernel/page.h>
#include <kernel/allocflags.h>
#include <kernel/attributes.h>
#include <kernel/spinlock.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>

#define HEAP_SEG_MAGIC_FREE      0x1308
#define HEAP_SEG_MAGIC_ALLOCATED 0x0505

typedef struct heap_segment {
	uintptr_t magic;
	size_t length;
	struct heap_segment *next;
	struct heap_segment *prev;
} heap_segment_t;

/**
 * @brief init the kernel heap
 */
void init_kheap(void);

/**
 * @brief allocate space in kernel heap
 * @param amount the amount of memory to allocate
 * @param flags flags to change the behaviour of the function, see ALLOC_FLAG_XXX constants
 * @return an pointer to the allocated region
 */
ATTR_MUST_USE ATTR_MALLOC void *kmalloc_flags(size_t amount, int flags) ATTR_OWNERSHIP_RETURN(kmalloc, 1);

/**
 * @brief allocate space in kernel heap
 * @param amount the amount of memory to allocate
 * @return a pointer to the allocated region or NULL on error
 */
ATTR_MUST_USE ATTR_MALLOC static inline void *kmalloc(size_t amount) ATTR_OWNERSHIP_RETURN(kmalloc, 1) {
	return kmalloc_flags(amount, 0);
}

/**
 * @brief free space in the kernel heap
 * @param ptr a pointer to the memory region to free
 */
void kfree(void *ptr) ATTR_OWNERSHIP_TAKE(kmalloc, 1);

/**
 * @brief reallocate a block with a different size
 * @param ptr the block to reallocate (can be NULL)
 * @param new_size the new size of the flag (can be 0)
 * @param flags flags to change the behaviour of the function, see ALLOC_FLAG_XXX constants
 * @return a pointer to the newly allocated region or NULL on error
 */
ATTR_MUST_USE ATTR_MALLOC void *krealloc_flags(void *ptr, size_t new_size, int flags);

/**
 * @brief reallocate a block with a different size
 * @param ptr the block to reallocate (can be NULL)
 * @param new_size the new size of the flag (can be 0)
 * @return a pointer to the newly allocated region or NULL on error
 */
ATTR_MUST_USE ATTR_MALLOC static inline void *krealloc(void *ptr, size_t new_size) {
	return krealloc_flags(ptr, new_size, 0);
}

#endif
