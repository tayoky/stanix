#include <kernel/pmm.h>
#include <kernel/mmu.h>

typedef struct pmm1_entry {
	struct pmm1_entry *next;
	size_t count;
} pmm1_entry_t;

static pmm1_entry_t *head = NULL;

// stage 1 allocator
uintptr_t pmm1_allocate_page(void) {
	if (head == NULL) return PAGE_INVALID;

	pmm1_entry_t *entry = head;
	uintptr_t page;
	if (entry->count > 1) {
		entry->count--;
		page = mmu_hhdm2phys(entry) + entry->count * PAGE_SIZE;
	} else {
		head = entry->next;
		page = mmu_hhdm2phys(entry);
	}
	return page;
}

void pmm1_add_free_pages(uintptr_t start, size_t count) {
	if (count == 0) return;
	kassert(start != PAGE_INVALID);
	kassert(start % PAGE_SIZE == 0);

	pmm1_entry_t *entry = mmu_phys2virt(start);
	entry->next = head;
	entry->count = count;
	head = entry;
}

int pmm1_get_free_pages(uintptr_t *start, size_t *count) {
	if (!head) return 0;
	pmm1_entry_t *entry = head;
	head = entry->next;
	*start = mmu_hhdm2phys(entry);
	*count = entry->count;
	return 1;
}
