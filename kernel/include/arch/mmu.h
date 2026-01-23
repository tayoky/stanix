#ifndef _KERNEL_MMU_H
#define _KERNEL_MMU_H

#include <kernel/arch.h>
#include <sys/mman.h>
#include <stddef.h>

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
 * @param physical_page the physical page it will be map
 * @param virtual_page the virtual page to map
 * @param flags flag to use for the virtual page (eg readonly, not executable...)
 */
void mmu_map_page(addrspace_t addrspace, uintptr_t physical_page, uintptr_t virtual_page, uint64_t flags);

/**
 * @brief unmap an page
 * @param addrspace an pointer to the address space
 * @param virtual_page the virtual page to unmap
 */
void mmu_unmap_page(addrspace_t addrspace, uintptr_t virtual_page);

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