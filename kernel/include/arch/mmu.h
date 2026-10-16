#ifndef KERNEL_MMU_H
#define KERNEL_MMU_H

#include <kernel/arch.h>
#include <kernel/macro.h>
#include <stddef.h>

// inspired by etheral's API

#define MMU_FLAG_PRESENT       0x1   // present in memory
#define MMU_FLAG_READ          0x2   // readable memory
#define MMU_FLAG_WRITE         0x4   // writable memory
#define MMU_FLAG_EXEC          0x8   // executable memory
#define MMU_FLAG_USER          0x10  // userspace memory
#define MMU_FLAG_GLOBAL        0x20  // global memory (hhdm, kerne, ...)
#define MMU_FLAG_ACCESS        0x40  // as been read
#define MMU_FLAG_DIRTY         0x80  // as been written
#define MMU_FLAG_WRITE_COMBINE 0x100 // combine writes
#define MMU_FLAG_UNCACHED      0x200 // disable caching

void init_mmu(void);

/**
 * @brief create an new addrspace and map all kernel modules and framebuffers
 * @return an pointer to the addrspace
 */
mmu_space_t mmu_space_init(void);

/**
 * @brief delete an address space and free all used pages by it (only by the table not the physical pages the address space point to)
 * @param addrspace an pointer to the address space to free/delete
 */
void mmu_space_destroy(mmu_space_t addrspace);

/**
 * @brief map a virtual pages to a physcal pages
 * @param addrspace an pointer to the addrspace
 * @param paddr the physical pages to map
 * @param vaddr the virtual pages to map
 * @param size the size of the region to map
 * @param flags flag to use for the virtual pages (eg readonly, not executable...)
 */
int mmu_map_pages(mmu_space_t addrspace, uintptr_t paddr, uintptr_t vaddr, size_t size, long flags);

/**
 * @brief map a mmu entry to a physcal page
 * @param addrspace an pointer to the addrspace
 * @param entry the entry to map
 * @param paddr the physical page to map
 * @param flags flag to use for the virtual page (eg readonly, not executable...)
 */
void mmu_entry_map(mmu_space_t addrspace, mmu_entry_t *entry, uintptr_t paddr, long flags);

/**
 * @brief unmap an entry
 * @param addrspace an pointer to the address space
 * @param entry the entry to unmap
 * @param vaddr the virtual address associed with this entry
 * @return the flags off this entry
 */
long mmu_entry_unmap(mmu_space_t addrspace, mmu_entry_t *entry, uintptr_t vaddr);

long mmu_entry_get_flags(mmu_space_t addrspace, mmu_entry_t *entry);
uintptr_t mmu_entry_get_page(mmu_space_t addrspace, mmu_entry_t *entry);
int mmu_entry_set_flags(mmu_space_t addrspace, mmu_entry_t *entry, long flags);
int mmu_entry_or_flags(mmu_space_t addrspace, mmu_entry_t *entry, long flags);
long mmu_entry_get_and_clear_flags(mmu_space_t addrspace, mmu_entry_t *entry, long flags);

mmu_entry_t *mmu_get_entry(mmu_space_t addrspace, uintptr_t vaddr);
mmu_entry_t *mmu_get_entry_and_acquire_lock(mmu_space_t addrspace, uintptr_t vaddr);

/**
 * @brief release lock on an entry and acquire the next one
 * @param addrspace the address space to get the new entry in
 * @param from where to start the search
 * @param vaddr where to store the vaddr of the found entry
 * @param old_entry the old entry to release the lock of
 * @return a pointer to the new entry or NULL if it doesn't exist
 */
mmu_entry_t *mmu_next_entry(mmu_space_t addrspace, uintptr_t from, uintptr_t *vaddr, mmu_entry_t *old_entry);
#define mmu_foreach_entry_range(vaddr, entry, addrspace, start, end) \
    loop_var(uintptr_t, vaddr) \
    for (mmu_entry_t *entry = mmu_next_entry(addrspace, start, &vaddr, NULL); \
    entry && vaddr < (end); \
    entry = mmu_next_entry(addrspace, vaddr, &vaddr, entry))

// TODO : implement this correcly
#define mmu_foreach_entry_and_acquire_lock_range(vaddr, entry, addrspace, start, end) mmu_foreach_entry_range(vaddr, entry, addrspace, start, end)

mmu_entry_t *mmu_create_entry_and_acquire_lock(mmu_space_t addrspace, uintptr_t vaddr);
mmu_entry_t *mmu_create_next_entry(mmu_space_t addrspace, uintptr_t vaddr, mmu_entry_t *old_entry, uintptr_t old_vaddr);
/*#define mmu_foreach_create_entry_range(vaddr, entry, addrspace, start, end) \
    loop_var_value(uintptr_t, vaddr, (start)) \
    for (mmu_entry_t *entry = mmu_create_entry_and_acquire_lock(addrspace, start); \
    vaddr < (end); \
    entry = mmu_create_next_entry(addrspace, vaddr + PAGE_SIZE, entry, vaddr), \
	vaddr += PAGE_SIZE)*/


void mmu_entry_release_lock(mmu_entry_t *entry);

/**
 * @brief map the kernel code and global data
 * @param addrspace an pointer to the address space
 */
void mmu_map_kernel(mmu_space_t addrspace);

/**
 * @brief map the hhdm section on a address space
 * @param addrspace an pointer to the address space
 */
void mmu_map_hhdm(mmu_space_t addrspace);

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
uintptr_t mmu_space_virt2phys(mmu_space_t addrspace, void *address);

extern uintptr_t hhdm_base;

/**
 * @brief turn a physical address into a virtual address via hhdm
 * @param address the physical address
 * @return a virtual address inside the hhdm
 */
static inline void *mmu_phys2virt(uintptr_t address) {
	return (void *)(hhdm_base + address);
}

/**
 * @brief turn a virtual address pointing to hhdm back into a physical address, faster than \ref mmu_virt2phys
 * @param address the virtual address
 * @return the physical address
 * @note if you cannot guarantee the virtual address is inside hhdm, use \ref mmu_virt2phys instead
 */
static inline uintptr_t mmu_hhdm2phys(void *address) {
    return (uintptr_t)address - hhdm_base;
}

/**
 * @brief set the base address of the hhdm
 * @param hhdm the base hhdm address
 */
void mmu_set_hhdm(uintptr_t hhdm);

/**
 * @brief get the current address space
 * @return the current address space
 */
mmu_space_t mmu_get_addr_space(void);

/**
 * @brief set the current address space
 * @param new_addrspace the new address space to set
 */
void mmu_set_addr_space(mmu_space_t new_addrspace);


#endif
