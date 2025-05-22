#include <kernel/pmm.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/limine.h>
#include <kernel/panic.h>
#include <kernel/page.h>
#include <kernel/spinlock.h>


void init_PMM(){
	kstatus("init PMM ... ");

	kernel->used_memory = kernel->total_memory;
	kernel->PMM_stack_head = NULL;

	for (uint64_t i = 0; i < kernel->memmap->entry_count; i++){
		if(kernel->memmap->entries[i]->type != LIMINE_MEMMAP_USABLE)continue;
		
		//find start and end and page align it
		kernel->memmap->entries[i]->length = PAGE_ALIGN_DOWN(kernel->memmap->entries[i]->length);
		uintptr_t start =  PAGE_ALIGN_UP(kernel->memmap->entries[i]->base);
		uintptr_t end = PAGE_ALIGN_DOWN(kernel->memmap->entries[i]->length + kernel->memmap->entries[i]->base);

		//when we page align it might become empty
		if(start == end){
			continue;
		}

		//create a new entry and push it to the top of the linked stack
		PMM_entry *entry = (PMM_entry *)(start  + kernel->hhdm);
		entry->size = (end - start)/PAGE_SIZE;
		entry->next = kernel->PMM_stack_head;
		kernel->PMM_stack_head = entry;

		//update used memory count
		kernel->used_memory -= PAGE_ALIGN_DOWN(kernel->memmap->entries[i]->length);
	}
	
	kok();
}

uintptr_t pmm_allocate_page(){
	spinlock_acquire(kernel->PMM_lock);

	//first : out of memory check
	if(!kernel->PMM_stack_head){
		panic("out of memory",NULL);
	}

	//take the head entry and (maybee) pop it
	uintptr_t page;
	if(kernel->PMM_stack_head->size > 1){
		page = ((uintptr_t)kernel->PMM_stack_head) - kernel->hhdm + (kernel->PMM_stack_head->size - 1) * PAGE_SIZE;
		kernel->PMM_stack_head->size--;
	} else {
		page = ((uintptr_t)kernel->PMM_stack_head) - kernel->hhdm;
		kernel->PMM_stack_head = kernel->PMM_stack_head->next;
	}
	kernel->used_memory += PAGE_SIZE;

	spinlock_release(kernel->PMM_lock);
	return page;
}

void pmm_free_page(uintptr_t page){
	spinlock_acquire(kernel->PMM_lock);

	kernel->used_memory -= PAGE_SIZE;
	PMM_entry *entry = (PMM_entry *)(page +  kernel->hhdm);
	entry->size = 1;
	entry->next = kernel->PMM_stack_head;
	kernel->PMM_stack_head = entry;

	spinlock_release(kernel->PMM_lock);
}