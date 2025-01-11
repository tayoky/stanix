#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>
#include "page.h"

/// @brief create an new PMLT4 and map all kernel modules and framebuffers
/// @param kernel an pointer to the kernel table
/// @return an pointer to the PMLT4
uint64_t *init_PMLT4(kernel_table *kernel);
void map_page(uint64_t *PMLT4,uint64_t physical_page,uint64_t virtual_page);
void unmap_page(uint64_t *PMLT4,uint64_t virtual_page);
void map_kernel(uint64_t *PMLT4);
void *virt2phys(kernel_table *kernel,void *address);

#ifndef NULL
#define NULL (void *)0
#endif

#define PAGING_ENTRY_ADDRESS (~(uint64_t)0XFFF)

#endif