#ifndef KHEAP_H
#define KHEAP_H

#include <kernel/mutex.h>
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
	mutex_t mutex;
	void (*changes_size)(ssize_t);
}heap_info;

#ifndef NULL
#define NULL ((void*)0)
#endif



/// @brief init the kernel heap
void init_kheap(void);

void change_kheap_size(ssize_t offset);

/// @brief allocate space in a heap
/// @param heap the heap to search memory into
/// @param amount the amount of memory to allocate
/// @return an pointer to the allocated region
void *malloc(heap_info *heap,size_t amount);

/// @brief free memory in a heap
/// @param heap the heap that contain the block of memory
/// @param ptr the pointer to the allocated region
void free(heap_info *heap,void *ptr);

/// @brief allocate space in kernel heap
/// @param amount the amount of memory to allocate
/// @return an pointer to the allocated region
void *kmalloc(size_t amount);

/// @brief free space in the kernel heap
/// @param ptr an pointer to the memory region to free
void kfree(void *ptr);

#define KHEAP_START (((uint64_t)0xFFFF << 48)  | ((uint64_t)(509 & 0x1FF) << 39))

#endif