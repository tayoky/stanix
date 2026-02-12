#ifndef KHEAP_H
#define KHEAP_H

#include <kernel/spinlock.h>
#include <kernel/page.h>
#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>

struct heap_segment_struct;

#define HEAP_SEG_MAGIC_FREE      0x1308
#define HEAP_SEG_MAGIC_ALLOCATED 0x0505

typedef struct heap_segment_struct{
	uint64_t magic;
	uint64_t lenght;
	struct heap_segment_struct *next;
	struct heap_segment_struct *prev;
}heap_segment;

typedef struct {
	uint64_t start;
	uint64_t lenght;
	heap_segment *first_seg;
	spinlock_t lock;
	void (*changes_size)(ssize_t);
}heap_info;

#ifndef NULL
#define NULL ((void*)0)
#endif

/// @brief init the kernel heap
void init_kheap(void);

void change_kheap_size(ssize_t offset);

/// @brief allocate space in kernel heap
/// @param amount the amount of memory to allocate
/// @return an pointer to the allocated region
void *kmalloc(size_t amount);

/// @brief free space in the kernel heap
/// @param ptr an pointer to the memory region to free
void kfree(void *ptr);

void *krealloc(void *ptr, size_t size);

#endif