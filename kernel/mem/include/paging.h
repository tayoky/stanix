#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>
#include "page.h"
#include "kernel.h"

void init_paging(kernel_table *kernel);
/// @brief create an new PMLT4 and map all kernel modules and framebuffers
/// @param kernel an pointer to the kernel table
/// @return an pointer to the PMLT4
uint64_t *init_PMLT4(kernel_table *kernel);

/// @brief delete an PMLT4 and free all used pages by it (only by the table not the physical pages the PT point to)
/// @param kernel an pointer to the kernel table
/// @param PMLT4 an pointer to the PMLT4 to free/delete
void delete_PMLT4(kernel_table *kernel,uint64_t *PMLT4);

/// @brief map an virtual page to a physcal page
/// @param kernel an pointer to the kernel table
/// @param PMLT4 an pointer to the PMLT4
/// @param physical_page the physical page it will be map
/// @param virtual_page the virtual page to map
/// @param flags flag to use for the virtual page (eg readonly, not executable...)
void map_page(kernel_table *kernel,uint64_t *PMLT4,uint64_t physical_page,uint64_t virtual_page,uint64_t flags);

/// @brief unmap an page
/// @param kernel an pointer to the kernel table
/// @param PMLT4 an pointer to the PMLT4
/// @param virtual_page the virtual page to unmap
void unmap_page(kernel_table *kernel,uint64_t *PMLT4,uint64_t virtual_page);

/// @brief map the kernel code and global data 
/// @param kernel an pointer to the kernel table
/// @param PMLT4 an pointer to the PMLT4
void map_kernel(kernel_table *kernel,uint64_t *PMLT4);

/// @brief map the hhdm section on a PMLT4
/// @param kernel an pointer to the kernel table
/// @param PMLT4 an pointer to the PMLT4
void map_hhdm(kernel_table *kernel,uint64_t *PMLT4);

/// @brief map the kheap on a PMLT4
/// @param kernel an pointer to the kernel table
/// @param PMLT4 an pointer to the PMLT4
void map_kheap(kernel_table *kernel,uint64_t *PMLT4);

/// @brief return the mapped physcal address of a virtual address
/// @param kernel an pointer to the kernel table
/// @param address the virtual address
/// @return the physical address it is mapped to
void *virt2phys(kernel_table *kernel,void *address);

#ifndef NULL
#define NULL (void *)0
#endif

#define PAGING_ENTRY_ADDRESS ((~(uint64_t)0XFFF)&(~(((uint64_t)0xFFF)<<52)))

#define PAGING_FLAG_READONLY_CPL0 0x01
#define PAGING_FLAG_RW_CPL0 0x03
#define PAGING_FLAG_READONLY_CPL3 0x05
#define PAGING_FLAG_RW_CPL3 0x07
#define PAGING_FLAG_NO_EXE ((uint64_t)1 << 61)

#endif