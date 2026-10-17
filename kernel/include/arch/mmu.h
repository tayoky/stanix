#ifndef KERNEL_MMU_H
#define KERNEL_MMU_H

#include <kernel/arch.h>
#include <kernel/macro.h>
#include <stddef.h>

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
 * @brief map virtual pages to physcal pages
 * @param addrspace an pointer to the addrspace
 * @param vaddr the virtual pages to map
 * @param paddr the physical pages to map
 * @param size the size of the region to map
 * @param flags flag to use for the virtual pages (eg readonly, not executable...)
 */
int mmu_map_pages(mmu_space_t addrspace, uintptr_t vaddr, uintptr_t paddr, size_t size, long flags);

/**
 * @brief allocate and map pages
 * @param addrspace an pointer to the addrspace
 * @param vaddr the virtual pages to map
 * @param size the size of the region to allocate and map
 * @param flags flag to use for the virtual pages (eg readonly, not executable...)
 */
int mmu_allocate_and_map_pages(mmu_space_t addrspace, uintptr_t vaddr, size_t size, long flags);


void mmu_unmap_pages(mmu_space_t addrspace, uintptr_t vaddr, size_t size);
void mmu_unmap_and_release_pages(mmu_space_t addrspace, uintptr_t vaddr, size_t size);

/**
 * @brief get a mmu entry of an address space
 * @param addrspace the addrspace to get the entry of
 * @param vaddr the vaddr to get an entry for
 * @return the entry if it exist or NULL
 * @note it might return a pointer than an empty entry so be sure to check,
 * the entry also become invalid when relasing the rcu lock
 */
mmu_entry_t *mmu_get_entry(mmu_space_t addrspace, uintptr_t vaddr);

/**
 * @brief get a mmu entry of an address space and acquire its lock
 * @param addrspace the addrspace to get the entry of
 * @param vaddr the vaddr to get an entry for
 * @return the entry if it exist or NULL
 * @note it might return a pointer than an empty entry so be sure to check,
 * the lock of entry must be released with \ref mmu_entry_release_lock
 */
mmu_entry_t *mmu_get_entry_and_acquire_lock(mmu_space_t addrspace, uintptr_t vaddr);

/**
 * @brief get or create	a mmu entry in an address space and acquire its lock
 * @param addrspace the addrspace to create an entry in
 * @param vaddr the vaddr to create an entey for
 * @return the entry or NULL if it could not be allocated
 * @note it might return a pointer than an empty entry so be sure to check,
 * the lock of entry must be released with \ref mmu_entry_release_lock
 */
mmu_entry_t *mmu_create_entry_and_acquire_lock(mmu_space_t PML4, uintptr_t vaddr);

/**
 * @brief get the next existing entry
 * @param addrspace the address space to get the new entry in
 * @param from where to start the search
 * @param vaddr where to store the vaddr of the found entry
 * @return a pointer to the new entry or NULL if it doesn't exist
 */
mmu_entry_t *mmu_get_next_entry(mmu_space_t addrspace, uintptr_t from, uintptr_t *vaddr);
#define mmu_foreach_entry_range(vaddr, entry, addrspace, start, end) \
    loop_var(uintptr_t, vaddr) \
    for (mmu_entry_t *entry = mmu_get_next_entry(addrspace, start, &vaddr); \
    entry && vaddr < (end); \
    entry = mmu_get_next_entry(addrspace, vaddr + PAGE_SIZE, &vaddr))


mmu_entry_t *mmu_get_next_entry_and_acquire_lock(mmu_space_t PML4, uintptr_t from, uintptr_t *vaddr, mmu_entry_t *old_entry, uintptr_t old_vaddr);
mmu_entry_t *mmu_create_next_entry_and_acquire_lock(mmu_space_t addrspace, uintptr_t vaddr, mmu_entry_t *old_entry, uintptr_t old_vaddr);

/**
 * @brief release the lock of an entry
 * @param entry the entry to release the lock of
 */
void mmu_entry_release_lock(mmu_entry_t *entry);

/**
 * @brief map a mmu entry to a physcal page
 * @param addrspace an pointer to the addrspace
 * @param entry the entry to map
 * @param paddr the physical page to map
 * @param flags flag to use for the virtual page (eg readonly, not executable...)
 * @note require the entry's lock
 */
void mmu_entry_map(mmu_space_t addrspace, mmu_entry_t *entry, uintptr_t paddr, long flags);

/**
 * @brief map a mmu entry to a physcal page and or flags
 * @param addrspace an pointer to the addrspace
 * @param entry the entry to map
 * @param paddr the physical page to map
 * @param flags flag to or for the virtual page (eg readonly, not executable...)
 * @note require the entry's lock
 */
void mmu_entry_map_and_or_flags(mmu_space_t addrspace, mmu_entry_t *entry, uintptr_t paddr, long flags);

/**
 * @brief unmap an entry
 * @param addrspace an pointer to the address space
 * @param entry the entry to unmap
 * @return the previous value of this entry
 * @note require the entry's lock
 */
mmu_entry_val_t mmu_entry_unmap(mmu_space_t addrspace, mmu_entry_t *entry);

long mmu_entry_get_flags(mmu_space_t addrspace, mmu_entry_t *entry);
uintptr_t mmu_entry_get_page(mmu_space_t addrspace, mmu_entry_t *entry);

/**
 * @brief set the flags of a mmu entry
 * @param addrspace the addrspace in which the entry is
 * @param flags the new flags for the entry
 * @return the previous value of this entry
 * @note require the entry's lock if changing the MMU_FLAG_PRESENT flag
 */
mmu_entry_val_t mmu_entry_set_flags(mmu_space_t addrspace, mmu_entry_t *entry, long flags);

/**
 * @brief or the flags of a mmu entry
 * @param addrspace the addrspace in which the entry is
 * @param flags the new flags to add to the entry
 * @return the previous value of this entry
 * @note require the entry's lock if changing the MMU_FLAG_PRESENT flag
 */
mmu_entry_val_t mmu_entry_or_flags(mmu_space_t addrspace, mmu_entry_t *entry, long flags);

/**
 * @brief clear flags of a mmu entry
 * @param addrspace the addrspace in which the entry is
 * @param flags the flags to clear on the entry
 * @return the previous value of this entry
 * @note require the entry's lock if changing the MMU_FLAG_PRESENT flag
 */
mmu_entry_val_t mmu_entry_clear_flags(mmu_space_t addrspace, mmu_entry_t *entry, long flags);

/**
 * @brief fetch the currenr value of an mmu entry
 * @param entry the entry to fetch the value of
 * @return the current value of the entry
 */
static inline mmu_entry_val_t mmu_entry_fetch(mmu_entry_t *entry) {
	return atomic_load(entry);
}

uintptr_t mmu_entry_val_get_page(mmu_entry_val_t entry_val);
long mmu_entry_val_get_flags(mmu_entry_val_t entry_val);

/**
 * @brief invalidate pages in the cache of the mmu
 * @param vaddr the base of the region to invalidate
 * @param size the size of the region to invalidate
 */
void mmu_invalidate(uintptr_t vaddr, size_t size);

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
