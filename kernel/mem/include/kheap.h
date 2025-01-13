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
	kheap_segment *first_seg;
}kheap_info;

#ifndef NULL
#define NULL ((void*)0)
#endif

struct kernel_table_struct;

/// @brief init the kernel heap
/// @param kernel an pointer to the kernel table
void init_kheap(struct kernel_table_struct *kernel);

void change_kheap_size(struct kernel_table_struct *kernel,int64_t offset);

void *kmalloc(struct kernel_table_struct *kernel,size_t amount);

void free(void *ptr);

#endif