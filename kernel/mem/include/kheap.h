#ifndef KHEAP_H
#define KHEAP_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
	uint64_t start;
	uint64_t lenght;
}kheap_info;

#ifndef NULL
#define NULL ((void*)0)
#endif

struct kernel_table_struct;

/// @brief init the kernel heap
/// @param kernel an pointer to the kernel table
void init_kheap(struct kernel_table_struct *kernel);

void change_kheap_size(struct kernel_table_struct *kernel,int64_t offset);

void *kmalloc(size_t amount);

void free(void *ptr);

#endif