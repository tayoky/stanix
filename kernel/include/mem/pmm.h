#ifndef KERNEL_PMM_H
#define KERNEL_PMM_H

#include <kernel/mmu.h>
#include <kernel/page.h>
#include <kernel/assert.h>
#include <sys/type.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdint.h>

#define ORDER_SIZE1  0
#define ORDER_SIZE2  1
#define ORDER_SIZE4  2
#define ORDER_SIZE8  3
#define ORDER_SIZE16 4
#define ORDERS_COUNT 5
#define ORDER2COUNT(order) (1UL << order)
typedef struct pmm {
	list_t entries[ORDERS_COUNT];
	spinlock_t lock;
} pmm_t;

typedef struct page {
	atomic_uint ref_count;
	atomic_uint flags;
	void *private;
	union {
		struct {
			// TODO : support 32 bits arch
			uint64_t lru_prev : 42;
			uint64_t offset : 42;
			uint64_t lru_next : 42;
			uint64_t reserved : 2;
		} __attribute__((packed)) cached;
		struct {
			int order;
		} pmm;
	};
} page_t;

#define PAGE_FLAG_DIRTY    0x1
#define PAGE_FLAG_BUSY     0x2 // page is busy (doing IO, ...)
#define PAGE_FLAG_USABLE   0x4 // the page is usable memory
#define PAGE_FLAG_RESERVED 0x8 // the page is reserved memory

#define ZONE_DEFAULT   42
#define ZONE_DMA24     0
#define ZONE_DMA32     1
#define ZONE_NORMAL    2
#define ZONE_EMERGENCY 3
#define ZONES_COUNT    4

#define ZONE_DMA24_END (1ULL << 16)
#define ZONE_DMA32_END (1ULL << 32)


/**
 * @brief get informations on a page
 * @param the address of the page to get informatiosn from
 * @return a pointer to the page's informations
 */
page_t *pmm_page_info(uintptr_t page);

/**
 * @brief figure out the zone to which a page belong
 * @param page the page to figure the zone of
 * @return the zone to which the page belong
 */
int pmm_get_zone(uintptr_t page);

/**
 * @brief init the PMM
 */
void init_pmm();

uintptr_t pmm_zone_allocate_pages(int zone, int order);

static inline uintptr_t pmm_zone_allocate_page(int zone){
	return pmm_zone_allocate_pages(zone, ORDER_SIZE1);
}

/**
 * @brief find an free page and allocate it
 * @return the physical address of the page
 */
static inline uintptr_t pmm_allocate_page(void) {
	return pmm_zone_allocate_page(ZONE_DEFAULT);
}

/**
 * @brief decrease ref count of a page and maybee free it
 * @param page the page to release
 */
void pmm_release_page(uintptr_t page);

/**
 * @brief unlike \ref pmm_release_page ignore ref count and direcly mark multiple pages as free
 * @param start the start of the order to mark as free, must be order aligned
 * @param order the order of pages from start to mark as free
 */
void pmm_set_free_pages(uintptr_t start, size_t order);

/**
 * @brief unlike \ref pmm_release_page ignore ref count and direcly mark multiple pages as free
 * @param start the start of the range to mark as free
 * @param count the number of pages from start to mark as free
 */
void pmm_set_free_pages_range(uintptr_t start, size_t count);

/**
 * @brief unlike \ref pmm_release_page ignore ref count and direcly mark page as free
 * @param page the page to mark as free
 */
static inline void pmm_set_free_page(uintptr_t page) {
	pmm_set_free_pages(start, ORDER_SIZE1);
}


uintptr_t pmm1_allocate_page(void);
void pmm1_add_free_pages(uintptr_t start, size_t count);
int pmm1_get_free_pages(uintptr_t *start, size_t *count);

/**
 * @brief setup the second stage of pmm (zero page, page_t info, ...)
 */
void init_second_stage_pmm(void);

/**
 * @brief check if a specified page is a usable page (usable, not free)
 * @param page the page to check
 * return 1 if usable else 0
 */
int pmm_is_page_usable(uintptr_t page);

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
 * @brief get the count of private pages
 * @return the count of private pages
 */
size_t pmm_get_private_pages(void);

/**
 * @brief get the count of shared pages
 * @return the count of shared pages
 */
size_t pmm_get_shared_pages(void);

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
int pmm_retain(uintptr_t page);

/**
 * @brief wait until a specified page has a masked flag equal to a value
 * @param mask the mask to apply to the flags
 * @param value the value the masked flags must be equal to
 * @return -EINTR if interrupted else 0
 */
int pmm_wait(uintptr_t page, unsigned int mask, unsigned int value);

#endif
