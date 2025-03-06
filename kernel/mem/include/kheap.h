#ifndef KHEAP_H
#define KHEAP_H
#include <stdint.h>
#include <stddef.h>

struct kheap_segment_struct;

#define KHEAP_SEG_MAGIC_FREE      0x1308
#define KHEAP_SEG_MAGIC_ALLOCATED 0x0505

typedef struct kheap_segment_struct{
	uint64_t magic;
	uint64_t lenght;
	struct kheap_segment_struct *next;
	struct kheap_segment_struct *prev;
}kheap_segment;

typedef struct {
	uint64_t start;
	uint64_t lenght;
	uint64_t *PDP;
	uint64_t PMLT4i;
	kheap_segment *first_seg;
	int lock;
}kheap_info;

#ifndef NULL
#define NULL ((void*)0)
#endif



/// @brief init the kernel heap
void init_kheap(void);

void change_kheap_size(int64_t offset);

/// @brief allocate space in kernel heap
/// @param amount the amount of memory to allocate
/// @return an pointer to the allocated region
void *kmalloc(size_t amount);

/// @brief free space in the kernel heap
/// @param ptr an pointer to the memory region to free
void kfree(void *ptr);

#define KHEAP_START (((uint64_t)0xFFFF << 48)  | ((uint64_t)(509 & 0x1FF) << 39))

#endif