#include <kernel/spinlock.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <kernel/bootinfo.h>
#include <kernel/assert.h>
#include <kernel/print.h>
#include <kernel/panic.h>
#include <kernel/page.h>
#include <kernel/pmm.h>

// inspired by linux's buddy allocator

#define PAGES_PER_SECTION (1UL << 14)
static page_t **page_info_sections = NULL;
static size_t used_pages = 0;
static size_t total_pages = 0;
static size_t private_pages;
static size_t shared_pages;
static uintptr_t zero_page = PAGE_INVALID;
static uintptr_t highest_page = 0;
static pmm_t pmms[ZONES_COUNT];
static int is_stage2 = 0;

#define PAGES_INFO_MMU_FLAGS MMU_FLAG_READ | MMU_FLAG_WRITE | MMU_FLAG_PRESENT | MMU_FLAG_GLOBAL

void init_pmm() {
	kstatusf("init PMM ... ");

	for (size_t i = 0; i < bootinfo_memmap_get_entries_count(); i++) {
		bootinfo_memmap_entry_t entry;
		bootinfo_memmap_get_entry(i, &entry);
		if (entry.type != MEMMAP_USABLE && entry.type != MEMMAP_BOOTLOADER && entry.type != MEMMAP_KERNEL) {
			continue;
		}

		// find start and end and page align it
		uintptr_t start =  PAGE_ALIGN_UP(entry.start);
		uintptr_t end = PAGE_ALIGN_DOWN(entry.start + entry.size);

		// when we page align it might become empty
		if (start >= end) {
			continue;
		}

		if (end - PAGE_SIZE > highest_page) {
			highest_page = end - PAGE_SIZE;
		}

		size_t pages_count = (end - start) / PAGE_SIZE;
		total_pages += pages_count;
		used_pages  += pages_count;
		if (entry.type != MEMMAP_USABLE) {
			continue;
		}

		pmm1_add_free_pages(start, pages_count);
	}
	kok();
}

static page_t **pmm_page_section(uintptr_t addr) {
	uintptr_t page_index = addr / PAGE_SIZE;
	return &page_info_sections[page_index / PAGES_PER_SECTION];
}

static page_t *pmm_allocate_page_section(uintptr_t *map_addr) {
	// we need to map new pages
	page_t *pages_info = (page_t*)*map_addr;
	for (size_t size=0; size<PAGES_PER_SECTION * sizeof(page_t); size+= PAGE_SIZE) {
		uintptr_t page = pmm_allocate_page();
		mmu_map_page(mmu_get_addr_space(), page, *map_addr, PAGES_INFO_MMU_FLAGS);
		*map_addr += PAGE_SIZE;
	}
	return pages_info;
}

void init_second_stage_pmm(void) {
	// allocate memory for sections array
	uintptr_t sections_start = MEM_PAGES_START;
	uintptr_t sections_end   = PAGE_ALIGN_UP  (MEM_PAGES_START + highest_page / PAGE_SIZE / PAGES_PER_SECTION * sizeof(page_t *)) + PAGE_SIZE;
	for (uintptr_t addr=sections_start; addr<sections_end; addr+= PAGE_SIZE) {
		// we need to map a new page
		uintptr_t page = pmm_allocate_page();
		kassert(page != PAGE_INVALID);
		mmu_map_page(mmu_get_addr_space(), page, addr, PAGES_INFO_MMU_FLAGS);
	}
	page_info_sections = (page_t**)MEM_PAGES_START;
	memset((void*)sections_start, 0, sections_end - sections_start);
	uintptr_t map_addr = sections_end;

	for (size_t i = 0; i < bootinfo_memmap_get_entries_count(); i++) {
		bootinfo_memmap_entry_t entry;
		bootinfo_memmap_get_entry(i, &entry);
		if (entry.type != MEMMAP_USABLE && entry.type != MEMMAP_BOOTLOADER && entry.type != MEMMAP_KERNEL) {
			continue;
		}

		// find start and end and page align it
		uintptr_t start =  PAGE_ALIGN_UP(entry.start);
		uintptr_t end = PAGE_ALIGN_DOWN(entry.start + entry.size);

		// when we page align it might become empty
		if (start >= end) {
			continue;
		}

		// now allocate memory for the struct pages
		uintptr_t section_start = start / (PAGES_PER_SECTION * PAGE_SIZE) * (PAGES_PER_SECTION * PAGE_SIZE);
		for (uintptr_t addr=section_start; addr<end; addr+= PAGES_PER_SECTION * PAGE_SIZE) {
			page_t **current = pmm_page_section(addr);
			if (*current) {
				// already allocated
				continue;
			}
			*current = pmm_allocate_page_section(&map_addr);
			kdebugf("allocate section for %p at %p\n", addr, *current);
		}

		// setup flags and ref count
		for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
			page_t *page_info = pmm_page_info(addr);
			page_info->flags = PAGE_FLAG_RESERVED;
			page_info->ref_count = 1;
		}
	}

	// now setup flags and ref count of free pages
	uintptr_t start;
	size_t count;
	while (pmm1_get_free_pages(&start, &count)) {
		for (size_t i=0; i<count; i++) {
			page_t *page_info = pmm_page_info(start + i * PAGE_SIZE);
			page_info->flags = PAGE_FLAG_USABLE;
		}
		pmm_set_free_pages_range(start, count);
	}
	is_stage2 = 1;
	zero_page = pmm_allocate_page();
	memset(mmu_phys2virt(zero_page), 0, PAGE_SIZE);
}

int pmm_is_page_usable(uintptr_t page) {
	if (page > highest_page) {
		return 0;
	}
	page_t *page_info = pmm_page_info(page);
	if (!page_info) {
		return 0;
	}
	if (page_info->flags & PAGE_FLAG_USABLE) {
		return 1;
	}
	return 0;
}

page_t *pmm_page_info(uintptr_t addr) {
	kassert(addr != PAGE_INVALID);
	if (!page_info_sections) return NULL;

	uintptr_t page_index = addr / PAGE_SIZE;
	page_t *pages_info = page_info_sections[page_index / PAGES_PER_SECTION];
	if (!pages_info) return NULL;
	return &pages_info[page_index % PAGES_PER_SECTION];
}

int pmm_get_zone(uintptr_t page) {
	if (page < ZONE_DMA16_END) {
		return ZONE_DMA16;
	} else if (page < ZONE_DMA32_END) {
		return ZONE_DMA32;
	} else {
		// TODO : ZONE_EMERGENCY
		return ZONE_NORMAL;
	}
}

static int pmm_is_free(page_t *page_info) {
	return atomic_load(&page_info->ref_count) == 0;
}

static uintptr_t pmm_raw_helper_allocate_pages(pmm_t *pmm, int order) {
	if (order >= ORDERS_COUNT) return PAGE_INVALID;
	list_node_t *node = pmm->entries[order];
	if (node) {
		uintptr_t pages = mmu_hhdm2phys(node);
		list_remove(&pmm->entries[order], &node);
		return pages;
	} else {
		// get pages from next order and split
		uintptr_t pages = pmm_raw_helper_allocate_pages(pmm, order + 1);
		if (pages == PAGE_INVALID) return PAGE_INVALID;
		// put back unused pages
		uintptr_t unused = pages + ORDER2COUNT(order) * PAGE_SIZE;
		page_t *unused_info = pmm_page_info(unused);
		kassert(unused_info);
		unused_info->pmm.order = order;
		node = mmu_phys2virt(unused);
		list_append(&pmm->entries[order], &node);
		return pages;
	}
}

static uintptr_t pmm_helper_allocate_pages(pmm_t *pmm, int order) {
	spinlock_acquire(&pmm->lock);
	uintptr_t pages = pmm_raw_helper_allocate_pages(pmm, order);
	if (pages != PAGE_INVALID) {
		used_pages += ORDER2COUNT(order);
		private_pages += ORDER2COUNT(order);
		for (size_t i=0; i<ORDER2COUNT(order); i++) {
			page_t *page_info = pmm_page_info(pages + i * PAGE_SIZE);
			kassert(page_info);
			atomic_store(&page_info->ref_count, 1);
		}
	}
	spinlock_release(&pmm->lock);
	return pages;
}

uintptr_t pmm_zone_allocate_pages(int zone, int order) {
	if (zone == ZONE_DEFAULT) zone = ZONE_NORMAL;
	if (!is_stage2) {
		// we can only allocate individual pages
		if (order != ORDER_SIZE1) return PAGE_INVALID;
		return pmm1_allocate_page();
	}

	while (zone >= 0){
		uintptr_t page = pmm_helper_allocate_pages(&pmms[zone], order);
		if (page != PAGE_INVALID) return page;
		zone--;
	}
	return PAGE_INVALID;
}

static void pmm_zone_set_free_pages(int zone, uintptr_t start, int order) {
	pmm_t *pmm = &pmms[zone];
	spinlock_acquire(&pmm->lock);
	used_pages -= ORDER2COUNT(order);

	// can we merge
	while (order + 1 < ORDERS_COUNT) {
		uintptr_t merging_page = start ^ (ORDER2COUNT(order) * PAGE_SIZE);
		page_t *merging_page_info = pmm_page_info(merging_page);
		if (!merging_page_info || !pmm_is_free(merging_page_info) || merging_page_info->pmm.order != order) {
			break;
		}
		// we can merge
		// remove from list
		list_node_t *node = mmu_phys2virt(merging_page);
		list_remove(&pmm->entries[order], node);
		
		if (merging_page < start) {
			start = merging_page;
		}
		order++;

	}
	list_node_t *node = mmu_phys2virt(start);
	list_append(&pmm->entries[order], node);

	page_t *page_info = pmm_page_info(start);
	page_info->pmm.order = order;

	spinlock_release(&pmm->lock);
}

void pmm_set_free_pages(uintptr_t start, size_t order) {
	kassert(start != PAGE_INVALID);
	kassert(start % (ORDER2COUNT(order) * PAGE_SIZE) == 0);

	// mark as free
	for (uintptr_t page=start; page<start+ORDER2COUNT(order)*PAGE_SIZE; page += PAGE_SIZE) {
		page_t *page_info = pmm_page_info(page);
		if (page_info) atomic_store(&page_info->ref_count, 0);
	}
	int zone = pmm_get_zone(start);
	pmm_zone_set_free_pages(zone, start, order);
}

void pmm_set_free_pages_range(uintptr_t start, size_t count) {
	if (count == 0) return;
	kassert(start != PAGE_INVALID);
	kassert(start % PAGE_SIZE == 0);
	while (count > 0) {
		for (int order=ORDERS_COUNT-1; order>=0; order--) {
			if (count < ORDER2COUNT(order)) continue;
			if (start % (ORDER2COUNT(order) * PAGE_SIZE)) continue;
			pmm_set_free_pages(start, order);
			count -= ORDER2COUNT(order);
			start += PAGE_SIZE * ORDER2COUNT(order);
			break;
		}
	}
}

void pmm_release_page(uintptr_t page) {
	kassert(page != PAGE_INVALID);
	kassert(page % PAGE_SIZE == 0);
	page_t *page_info = pmm_page_info(page);
	if (page_info) {
		size_t ref = atomic_fetch_sub(&page_info->ref_count, 1);
		if (ref != 1) {
			// ref remaning
			if (ref == 2) {
				// we go from shared to private
				shared_pages--;
				private_pages++;
			}
			return;
		}
	}
	private_pages--;
	pmm_set_free_page(page);
}

int pmm_retain(uintptr_t page) {
	kassert(page != PAGE_INVALID);
	kassert(page % PAGE_SIZE == 0);
	page_t *page_info = pmm_page_info(page);
	kassert(page_info);
	unsigned int old = atomic_load(&page_info->ref_count);
	while (old != 0) {
		if (atomic_compare_exchange_weak(&page_info->ref_count, &old, old + 1)) {
			if (old == 1) {
				// we got from private to shared
				private_pages--;
				shared_pages++;
			}
			return 1;
		}
		// we raced and need to retry
	}
	return 0;
}

uintptr_t pmm_dup_page(uintptr_t page) {
	kassert(page != PAGE_INVALID);
	kassert(page % PAGE_SIZE == 0);
	uintptr_t new_page = pmm_allocate_page();
	if (new_page == PAGE_INVALID) return PAGE_INVALID;
	memcpy(mmu_phys2virt(new_page), mmu_phys2virt(page), PAGE_SIZE);
	return new_page;
}

int pmm_wait(uintptr_t page, unsigned int mask, unsigned int value) {
	// TODO
	(void)page;
	(void)mask;
	(void)value;
	return 0;
}

uintptr_t pmm_get_zero_page(void) {
	pmm_retain(zero_page);
	return zero_page;
}

size_t pmm_get_used_pages(void) {
	return used_pages;
}

size_t pmm_get_total_pages(void) {
	return total_pages;
}

size_t pmm_get_private_pages(void) {
	return private_pages;
}

size_t pmm_get_shared_pages(void) {
	return shared_pages;
}
