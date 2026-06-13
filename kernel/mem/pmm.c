#include <kernel/spinlock.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <kernel/bootinfo.h>
#include <kernel/assert.h>
#include <kernel/print.h>
#include <kernel/panic.h>
#include <kernel/page.h>
#include <kernel/pmm.h>

// inspired by linux's struct page

#define PAGES_PER_SECTION (1UL << 14)
static page_t **page_info_sections = NULL;
static pmm_entry_t *stack_head;
static size_t used_pages;
static size_t total_pages;
static size_t private_pages;
static size_t shared_pages;
static spinlock_t pmm_lock;
static uintptr_t zero_page = PAGE_INVALID;
static uintptr_t highest_page = 0;

#define PAGES_INFO_MMU_FLAGS MMU_FLAG_READ | MMU_FLAG_WRITE | MMU_FLAG_PRESENT | MMU_FLAG_GLOBAL

void init_PMM() {
	kstatusf("init PMM ... ");

	stack_head = NULL;
	used_pages = 0;
	total_pages = 0;
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

		pmm_set_free_pages(start, pages_count);
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
	static uintptr_t map_addr = sections_end;

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
		}

		// setup flags
		for (uintptr_t addr=start; addr<end; addr += PAGE_SIZE) {
			pmm_page_info(addr).flags = PAGE_FLAG_USABLE;
		}
	}
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

uintptr_t pmm_allocate_page(void) {
	spinlock_acquire(&pmm_lock);

	// first : out of memory check
	if (!stack_head) {
		spinlock_release(&pmm_lock);
		return PAGE_INVALID;
	}

	// take the head entry and (maybee) pop it
	uintptr_t page;
	if (stack_head->size > 1) {
		page = (uintptr_t)mmu_hhdm2phys(stack_head) + (stack_head->size - 1) * PAGE_SIZE;
		stack_head->size--;
	} else {
		page = (uintptr_t)mmu_hhdm2phys(stack_head);
		stack_head = stack_head->next;
	}
	used_pages++;
	private_pages++;

	page_t *page_info = pmm_page_info(page);
	if (page_info) {
		atomic_store(&page_info->ref_count, 1);
	}
	spinlock_release(&pmm_lock);
	return page;
}

void pmm_set_free_pages(uintptr_t start, size_t count) {
	if (count == 0) return;
	kassert(start != PAGE_INVALID);
	spinlock_acquire(&pmm_lock);

	used_pages -= count;
	pmm_entry_t *entry = mmu_phys2virt(start);
	entry->size = count;
	entry->next = stack_head;
	stack_head = entry;

	spinlock_release(&pmm_lock);
}

void pmm_release_page(uintptr_t page) {
	kassert(page != PAGE_INVALID);
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
	page_t *page_info = pmm_page_info(page);
	size_t old = atomic_load(&page_info->ref_count);
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
	uintptr_t new_page = pmm_allocate_page();
	if (new_page == PAGE_INVALID) return PAGE_INVALID;
	memcpy(mmu_phys2virt(new_page), mmu_phys2virt(page), PAGE_SIZE);
	return new_page;
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
