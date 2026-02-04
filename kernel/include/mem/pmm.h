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
	atomic_long flags;
} page_t;

#define PAGE_FLAG_DIRTY 0x1
#define PAGE_FLAG_READY 0x2

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
 * @return the physical address of the page
 */
uintptr_t pmm_allocate_page();

/**
 * @brief decrease ref count of a page and maybee free it
 * @param page the page to release
 */
void pmm_free_page(uintptr_t page);


/**
 * @brief unlike \ref pmm_free_page ignore ref count and direcly mark page as free
 * @param page the page to mark as free
 */
void pmm_set_free_page(uintptr_t page);

/**
 * @brief setup the second stage of pmm (zero page, page_t info, ...)
 */
void init_second_stage_pmm(void);

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

/**
 * @brief duplicate a page
 * @param page the physical address of the page to duplicate
 * @return the physical address of the new page
 */
uintptr_t pmm_dup_page(uintptr_t page);

/**
 * @brief get and add a refcount to a page alaways full of zero
 * @return the physcial address of the zero page
 */
uintptr_t pmm_get_zero_page(void);

/**
 * @brief add a ref count to a page if the page has a non null ref count
 * @param page the page to hold
 * @return 1 if succed or 0 if ref count is null
 */
static inline int pmm_retain(uintptr_t page) {
	page_t *page_info = pmm_page_info(page);
	size_t old = atomic_load(&page_info->ref_count);
    while (old != 0) {
        if (atomic_compare_exchange_weak(&page_info->ref_count, &old, old + 1)) {
            return 1;
		}
        // we raced and need to retry
    }
    return 0;
}

#endif
