#include <kernel/pmm.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/limine.h>
#include <kernel/panic.h>
#include <kernel/page.h>
#include <kernel/spinlock.h>

static page_t *pages_info = NULL;
static pmm_entry_t *stack_head;
static size_t used_pages;
static size_t total_pages;
static spinlock_t pmm_lock;

#define PAGES_INFO_MMU_FLAGS MMU_FLAG_READ | MMU_FLAG_WRITE | MMU_FLAG_PRESENT | MMU_FLAG_GLOBAL

void init_PMM() {
	kstatusf("init PMM ... ");

	stack_head = NULL;
	used_pages = 0;
	total_pages = 0;
	for (uint64_t i = 0; i < kernel->memmap->entry_count; i++) {
		uint64_t type = kernel->memmap->entries[i]->type;
		if (type != LIMINE_MEMMAP_USABLE && type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE && type != LIMINE_MEMMAP_KERNEL_AND_MODULES) {
			continue;
		}
		total_pages += kernel->memmap->entries[i]->length / PAGE_SIZE;
		used_pages += kernel->memmap->entries[i]->length / PAGE_SIZE;
		if (type != LIMINE_MEMMAP_USABLE) {
			continue;
		}

		// find start and end and page align it
		uintptr_t start =  PAGE_ALIGN_UP(kernel->memmap->entries[i]->base);
		uintptr_t end = PAGE_ALIGN_DOWN(kernel->memmap->entries[i]->length + kernel->memmap->entries[i]->base);

		// when we page align it might become empty
		if (start >= end) {
			continue;
		}

		// create a new entry and push it to the top of the linked stack
		pmm_entry_t *entry = (pmm_entry_t *)(start + kernel->hhdm);
		entry->size = (end - start) / PAGE_SIZE;
		entry->next = stack_head;
		stack_head = entry;

		// update used memory count
		used_pages -= (end - start) / PAGE_SIZE;
	}
	kok();
}

void pmm_map_info(addrspace_t addr_space) {
	for (uint64_t i = 0; i < kernel->memmap->entry_count; i++) {
		uint64_t type = kernel->memmap->entries[i]->type;
		if (type != LIMINE_MEMMAP_USABLE && type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE && type != LIMINE_MEMMAP_KERNEL_AND_MODULES) {
			continue;
		}

		// find start and end and page align it
		uintptr_t start =  PAGE_ALIGN_UP(kernel->memmap->entries[i]->base);
		uintptr_t end = PAGE_ALIGN_DOWN(kernel->memmap->entries[i]->length + kernel->memmap->entries[i]->base);

		// when we page align it might become empty
		if (start >= end) {
			continue;
		}

		// now allocate memory for the struct pages
		uintptr_t pages_start = PAGE_ALIGN_DOWN(MEM_PAGES_START + start / PAGE_SIZE * sizeof(page_t));
		uintptr_t pages_end   = PAGE_ALIGN_UP  (MEM_PAGES_START + end   / PAGE_SIZE * sizeof(page_t));
		kdebugf("map %lx to %lx\n", pages_start, pages_end);
		for (uintptr_t addr = pages_start; addr < pages_end; addr += PAGE_SIZE) {
			if (mmu_virt2phys((void*)addr) == PAGE_INVALID) {
				// we need to map a new page
				uintptr_t page = pmm_allocate_page();
				mmu_map_page(addr_space, page, addr, PAGES_INFO_MMU_FLAGS);
			}
		}
	}
	pages_info = (page_t*)MEM_PAGES_START;
}

page_t *pmm_page_info(uintptr_t addr) {
	return &pages_info[addr >> PAGE_SHIFT];
}

uintptr_t pmm_allocate_page(void) {
	spinlock_acquire(&pmm_lock);

	// first : out of memory check
	if (!stack_head) {
		return PAGE_INVALID;
	}

	// take the head entry and (maybee) pop it
	uintptr_t page;
	if (stack_head->size > 1) {
		page = ((uintptr_t)stack_head) - kernel->hhdm + (stack_head->size - 1) * PAGE_SIZE;
		stack_head->size--;
	} else {
		page = ((uintptr_t)stack_head) - kernel->hhdm;
		stack_head = stack_head->next;
	}
	used_pages++;

	if (pages_info) {
		atomic_store(&pmm_page_info(page)->ref_count, 1);
	}
	spinlock_release(&pmm_lock);
	return page;
}

void pmm_free_page(uintptr_t page) {
	if (pages_info) {
		if (atomic_fetch_sub(&pmm_page_info(page)->ref_count, 1) != 1) {
			// ref remaning
			return;
		}
	}
	spinlock_acquire(&pmm_lock);

	used_pages--;
	pmm_entry_t *entry = (pmm_entry_t *)(page + kernel->hhdm);
	entry->size = 1;
	entry->next = stack_head;
	stack_head = entry;

	spinlock_release(&pmm_lock);
}

size_t pmm_get_used_pages(void) {
	return used_pages;
}

size_t pmm_get_total_pages(void) {
	return total_pages;
}
