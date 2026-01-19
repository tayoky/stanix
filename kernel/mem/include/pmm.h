#ifndef BITMAP_H
#define BITMAP_H

#include <kernel/paging.h>
#include <kernel/page.h>
#include <sys/type.h>
#include <stdint.h>
#include <limits.h>
#include <stdatomic.h>

typedef struct PMM_entry_struct {
	struct PMM_entry_struct *next;
	size_t size; //size of the chunk in page
} PMM_entry;

typedef struct page {
	atomic_size_t ref_count;
} page_t;

#define PMM_INVALID_PAGE ULONG_MAX - PAGE_SIZE + 1

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
void map_PMM_info(addrspace_t addr_space);

#endif