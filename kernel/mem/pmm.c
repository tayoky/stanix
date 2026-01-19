#include <kernel/pmm.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/limine.h>
#include <kernel/panic.h>
#include <kernel/page.h>
#include <kernel/spinlock.h>

static page_t *pages_info = NULL;

void init_PMM() {
	kstatusf("init PMM ... ");

	kernel->used_memory = kernel->total_memory;
	kernel->PMM_stack_head = NULL;

	for (uint64_t i = 0; i < kernel->memmap->entry_count; i++) {
		if (kernel->memmap->entries[i]->type != LIMINE_MEMMAP_USABLE)continue;

		//find start and end and page align it
		uintptr_t start =  PAGE_ALIGN_UP(kernel->memmap->entries[i]->base);
		uintptr_t end = PAGE_ALIGN_DOWN(kernel->memmap->entries[i]->length + kernel->memmap->entries[i]->base);

		//when we page align it might become empty
		if (start >= end) {
			continue;
		}

		//create a new entry and push it to the top of the linked stack
		PMM_entry *entry = (PMM_entry *)(start + kernel->hhdm);
		entry->size = (end - start) / PAGE_SIZE;
		entry->next = kernel->PMM_stack_head;
		kernel->PMM_stack_head = entry;

		//update used memory count
		kernel->used_memory -= PAGE_ALIGN_DOWN(kernel->memmap->entries[i]->length);
	}
	kok();
}

void map_PMM_info(addrspace_t addr_space) {
	for (uint64_t i = 0; i < kernel->memmap->entry_count; i++) {
		if (kernel->memmap->entries[i]->type != LIMINE_MEMMAP_USABLE)continue;

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
			if (!virt2phys((void*)addr)) {
				// we need to map a new page
				uintptr_t page = pmm_allocate_page();
				map_page(addr_space, page, addr, PAGING_FLAG_RW_CPL0 | PAGING_FLAG_NO_EXE);
			}
		}
	}
	pages_info = (page_t*)MEM_PAGES_START;
}

page_t *pmm_page_info(uintptr_t addr) {
	return &pages_info[addr >> PAGE_SHIFT];
}

uintptr_t pmm_allocate_page(void) {
	spinlock_acquire(&kernel->PMM_lock);

	// first : out of memory check
	if (!kernel->PMM_stack_head) {
		return PMM_INVALID_PAGE;
	}

	// take the head entry and (maybee) pop it
	uintptr_t page;
	if (kernel->PMM_stack_head->size > 1) {
		page = ((uintptr_t)kernel->PMM_stack_head) - kernel->hhdm + (kernel->PMM_stack_head->size - 1) * PAGE_SIZE;
		kernel->PMM_stack_head->size--;
	} else {
		page = ((uintptr_t)kernel->PMM_stack_head) - kernel->hhdm;
		kernel->PMM_stack_head = kernel->PMM_stack_head->next;
	}
	kernel->used_memory += PAGE_SIZE;

	if (pages_info) {
		pmm_page_info(page)->ref_count = 1;
	}
	spinlock_release(&kernel->PMM_lock);
	return page;
}

void pmm_free_page(uintptr_t page) {
	spinlock_acquire(&kernel->PMM_lock);

	kernel->used_memory -= PAGE_SIZE;
	PMM_entry *entry = (PMM_entry *)(page + kernel->hhdm);
	entry->size = 1;
	entry->next = kernel->PMM_stack_head;
	kernel->PMM_stack_head = entry;

	spinlock_release(&kernel->PMM_lock);
}