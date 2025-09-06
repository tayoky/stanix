#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>
#include <kernel/page.h>
#include <kernel/kernel.h>

void init_paging(void);
/// @brief create an new PMLT4 and map all kernel modules and framebuffers
/// @return an pointer to the PMLT4
uint64_t *create_addr_space();

/// @brief delete an PMLT4 and free all used pages by it (only by the table not the physical pages the PT point to)
/// @param PMLT4 an pointer to the PMLT4 to free/delete
void delete_addr_space(uint64_t *PMLT4);

/// @brief map an virtual page to a physcal page
/// @param PMLT4 an pointer to the PMLT4
/// @param physical_page the physical page it will be map
/// @param virtual_page the virtual page to map
/// @param flags flag to use for the virtual page (eg readonly, not executable...)
void map_page(uint64_t *PMLT4,uint64_t physical_page,uint64_t virtual_page,uint64_t flags);

/// @brief unmap an page
/// @param PMLT4 an pointer to the PMLT4
/// @param virtual_page the virtual page to unmap
void unmap_page(uint64_t *PMLT4,uint64_t virtual_page);

/// @brief map the kernel code and global data 
/// @param PMLT4 an pointer to the PMLT4
void map_kernel(uint64_t *PMLT4);

/// @brief map the hhdm section on a PMLT4
/// @param PMLT4 an pointer to the PMLT4
void map_hhdm(uint64_t *PMLT4);

/// @brief return the mapped physcal address of a virtual address
/// @param address the virtual address
/// @return the physical address it is mapped to
void *virt2phys(void *address);

/// @brief return the mapped physcal address of a virtual address in any PMLT4
/// @param PMLT4 an pointer to the PMLT4
/// @param address the virtual address
/// @return the physical address it is mapped to
void *space_virt2phys(uint64_t *PMLT4,void *address);

/// @brief get the current address space
/// @return the current address space
uint64_t get_addr_space();

#ifndef NULL
#define NULL (void *)0
#endif

#define PAGING_ENTRY_ADDRESS ((~(uint64_t)0XFFF)&(~(((uint64_t)0xFFF)<<52)))

#define PAGING_FLAG_READONLY_CPL0 0x01
#define PAGING_FLAG_RW_CPL0 0x03
#define PAGING_FLAG_READONLY_CPL3 0x05
#define PAGING_FLAG_RW_CPL3 0x07
#define PAGING_FLAG_NO_EXE ((uint64_t)1 << 61)

#define KERNEL_STACK_SIZE (64 * 1024)

#define USER_STACK_TOP 0x80000000000
#define USER_STACK_SIZE 64 *PAGE_SIZE
#define USER_STACK_BOTTOM USER_STACK_TOP - USER_STACK_SIZE

#define USERSPACE_LIMIT (uintptr_t)0x7FFFFFFFFFFFF

#define switch_stack() asm volatile("mov $0, %%rbp\nmov %0, %%rsp" : :)

#endif