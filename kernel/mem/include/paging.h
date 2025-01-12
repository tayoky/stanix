#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>
#include "page.h"

void init_paging(kernel_table *kernel);
/// @brief create an new PMLT4 and map all kernel modules and framebuffers
/// @param kernel an pointer to the kernel table
/// @return an pointer to the PMLT4
uint64_t *init_PMLT4(kernel_table *kernel);
void map_page(kernel_table *kernel,uint64_t *PMLT4,uint64_t physical_page,uint64_t virtual_page,uint8_t flags);
void unmap_page(kernel_table *kernel,uint64_t *PMLT4,uint64_t virtual_page);
void map_kernel(kernel_table *kernel,uint64_t *PMLT4);
void map_hhdm(kernel_table *kernel,uint64_t *PMLT4);
void *virt2phys(kernel_table *kernel,void *address);

#ifndef NULL
#define NULL (void *)0
#endif

#define PAGING_ENTRY_ADDRESS ((~(uint64_t)0XFFF)&(~(((uint64_t)0xFFF)<<52)))

#define PAGING_FLAG_READONLY_CPL0 0x01
#define PAGING_FLAG_RW_CPL0 0x03
#define PAGING_FLAG_READONLY_CPL3 0x05
#define PAGING_FLAG_RW_CPL3 0x07

#endif