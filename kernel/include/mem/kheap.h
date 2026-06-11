#ifndef KERNEL_KHEAP_H
#define KERNEL_KHEAP_H

#include <kernel/page.h>
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
 * @return an pointer to the allocated region
 */
void *kmalloc(size_t amount);

/**
 * @brief free space in the kernel heap
 * @param ptr an pointer to the memory region to free
 */
void kfree(void *ptr);

void *krealloc(void *ptr, size_t size);

#endif
