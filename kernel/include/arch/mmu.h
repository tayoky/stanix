#ifndef _KERNEL_MMU_H
#define _KERNEL_MMU_H

#include <kernel/arch.h>
#include <stddef.h>

// inspired by etheral's API

#define MMU_FLAG_PRESENT 0x1  // present in memory
#define MMU_FLAG_READ    0x2  // readable memory
#define MMU_FLAG_WRITE   0x4  // writable memory
#define MMU_FLAG_EXEC    0x8  // executable memory
#define MMU_FLAG_USER    0x10 // userspace memory
#define MMU_FLAG_GLOBAL  0x20 // global memory (hhdm, kerne, ...)
#define MMU_FLAG_ACCESS  0x40
#define MMU_FLAG_DIRTY   0x80

void init_mmu(void);

/**
 * @brief create an new addrspace and map all kernel modules and framebuffers
 * @return an pointer to the addrspace
 */
addrspace_t mmu_create_addr_space();

/**
 * @brief delete an address space and free all used pages by it (only by the table not the physical pages the address space point to)
 * @param addrspace an pointer to the address space to free/delete
 */
void mmu_delete_addr_space(addrspace_t addrspace);

/**
 * @brief map an virtual page to a physcal page
 * @param addrspace an pointer to the addrspace
 * @param paddr the physical page it will be map
 * @param vaddr the virtual page to map
 * @param flags flag to use for the virtual page (eg readonly, not executable...)
 */
void mmu_map_page(addrspace_t addrspace, uintptr_t paddr, uintptr_t vaddr, long flags);

/**
 * @brief unmap an page
 * @param addrspace an pointer to the address space
 * @param vaddr the virtual page to unmap
 */
void mmu_unmap_page(addrspace_t addrspace, uintptr_t vaddr);

long mmu_get_flags(addrspace_t addrspace, uintptr_t vaddr);
int mmu_set_flags(addrspace_t addrspace, uintptr_t vaddr, long flags);

/**
 * @brief map the kernel code and global data
 * @param addrspace an pointer to the address space
 */
void mmu_map_kernel(addrspace_t addrspace);

/**
 * @brief map the hhdm section on a address space
 * @param addrspace an pointer to the address space
 */
void mmu_map_hhdm(addrspace_t addrspace);

/**
 * @brief return the mapped physcal address of a virtual address
 * @param address the virtual address
 * @return the physical address it is mapped to
 */
uintptr_t mmu_virt2phys(void *address);

/**
 * @brief return the mapped physcal address of a virtual address in any address space
 * @param addrspace an pointer to the address space
 * @param address the virtual address
 * @return the physical address it is mapped to
 */
uintptr_t mmu_space_virt2phys(addrspace_t addrspace, void *address);

/**
 * @brief get the current address space
 * @return the current address space
 */
addrspace_t mmu_get_addr_space();

/**
 * @brief set the current address space
 * @param new_addrspace the new address space to set
 */
void mmu_set_addr_space(addrspace_t new_addrspace);


#endif