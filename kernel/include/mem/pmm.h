#ifndef _KERNEL_PMM_H
#define _KERNEL_PMM_H

#include <kernel/page.h>
#include <kernel/mmu.h>
#include <sys/type.h>
#include <stdint.h>
#include <limits.h>
#include <stdatomic.h>

typedef struct pmm_entry {
	struct pmm_entry *next;
	size_t size; //size of the chunk in page
} pmm_entry_t;

typedef struct page {
	atomic_size_t ref_count;
} page_t;

/**
 * @brief get informations on a page
 * @param the address of the page to get informatiosn from
 * @return a pointer to the page's informations
 */
page_t *pmm_page_info(uintptr_t page);

/**
 * @brief init the PMM
 */
void init_PMM();

/**
 * @brief find an free page and allocate it
 * @return the address of the page (multiply it by 0x1000 to get it's real address)
 */
uintptr_t pmm_allocate_page();

/**
 * @brief mark an page as allocated
 * @param page the page to mark as allocated
 */
void pmm_free_page(uintptr_t page);

/**
 * @brief map the pmm's page_t
 * @param addr_space the address space to map into
 */
void pmm_map_info(addrspace_t addr_space);

/**
 * @brief get the amount of currently used physical pages
 * @return count of currently used pages
 */
size_t pmm_get_used_pages(void);

/**
 * @brief get the amount of total physical pages
 * @return count of physical pages
 */
size_t pmm_get_total_pages(void);

#endif