#include "paging.h"
#include "kheap.h"
#include "print.h"
#include "panic.h"
#include "spinlock.h"

void init_kheap(void){
	kstatus("init kheap... ");

	spinlock_release(kernel->kheap.lock);

	kernel->kheap.start = PAGE_ALIGN_DOWN(KHEAP_START);

	//get cr3
	uint64_t cr3;
	asm("mov %%cr3, %%rax" : "=a" (cr3));
	uint64_t *PMLT4 = (uint64_t *)(cr3 + kernel->hhdm);
	map_page(PMLT4,allocate_page(&kernel->bitmap),kernel->kheap.start/PAGE_SIZE,PAGING_FLAG_RW_CPL0 | PAGING_FLAG_NO_EXE);

	//get the PDP for the kernel heap
	kernel->kheap.PMLT4i = (kernel->kheap.start >> 39) & 0x1FF;
	kernel->kheap.PDP = (uint64_t *)(PMLT4[kernel->kheap.PMLT4i] & PAGING_ENTRY_ADDRESS);

	kernel->kheap.lenght = PAGE_SIZE;

	//init the first seg
	kernel->kheap.first_seg = (kheap_segment *)kernel->kheap.start;
	kernel->kheap.first_seg->magic = KHEAP_SEG_MAGIC_FREE;
	kernel->kheap.first_seg->lenght = kernel->kheap.lenght - sizeof(kheap_segment);
	kernel->kheap.first_seg->prev = NULL;
	kernel->kheap.first_seg->next = NULL;

	kok();
}

void change_kheap_size(int64_t offset){
	if(!offset)return;
	int64_t offset_page = offset / PAGE_SIZE;

	//get the PMLT4
	uint64_t cr3;
	asm("mov %%cr3, %%rax" : "=a" (cr3));
	uint64_t *PMLT4 = (uint64_t *)(cr3 + kernel->hhdm);

	if(offset < 0){
		//make kheap smaller
		for (int64_t i = 0; i > offset_page; i--){
			uint64_t virt_page = (kernel->kheap.start + kernel->kheap.lenght)/PAGE_SIZE + i;
			uint64_t phys_page = (uint64_t)virt2phys((void *)(virt_page*PAGE_SIZE)) / PAGE_SIZE;
			unmap_page(PMLT4,virt_page);
			free_page(&kernel->bitmap,phys_page);
		}
	} else {
		//make kheap bigger
		for (int64_t i = 0; i < offset_page; i++){
			map_page(PMLT4,allocate_page(&kernel->bitmap),(kernel->kheap.start+kernel->kheap.lenght)/PAGE_SIZE+i,PAGING_FLAG_RW_CPL0 | PAGING_FLAG_NO_EXE);
		}
	}
	kernel->kheap.lenght += offset_page * PAGE_SIZE;
}

void show_memseg(){
	kheap_segment *current_seg = kernel->kheap.first_seg;
	char *labels[] ={
		"free     ",
		"allocated"
	};
	while (current_seg){
		kdebugf("memseg %s start : 0x%lx lenght : 0x%lx\n",labels[(current_seg->magic == KHEAP_SEG_MAGIC_ALLOCATED)],current_seg,current_seg->lenght);
		current_seg = current_seg->next;
	}
}

void *kmalloc(size_t amount){
	spinlock_acquire(kernel->kheap.lock);
	kheap_segment *current_seg = kernel->kheap.first_seg;

	while (current_seg->lenght < amount || current_seg->magic != KHEAP_SEG_MAGIC_FREE){
		if(current_seg->next == NULL){
			//no more segment need to make kheap bigger
			change_kheap_size(PAGE_ALIGN_UP(amount - current_seg->lenght + sizeof(kheap_segment) + 1));
			current_seg->lenght = amount + sizeof(kheap_segment) + 1;
			break;
		}
		current_seg = current_seg->next;
	}

	//we find a good segment

	//if big enougth cut it
	if(current_seg->lenght >= amount + sizeof(kheap_segment) + 1){
		//big enougth
		kheap_segment *new_seg = (kheap_segment *)((uint64_t)current_seg + amount + sizeof(kheap_segment));
		new_seg->lenght = current_seg->lenght - (sizeof(kheap_segment) + amount);
		current_seg->lenght = amount;
		new_seg->magic = KHEAP_SEG_MAGIC_FREE;

		if(current_seg->next){
			current_seg->next->prev = new_seg;
		}
		new_seg->next = current_seg->next;
		new_seg->prev = current_seg;
		current_seg->next = new_seg;
	}

	current_seg->magic = KHEAP_SEG_MAGIC_ALLOCATED;
	spinlock_release(kernel->kheap.lock);
	return (void *)current_seg + sizeof(kheap_segment);
}

void kfree(void *ptr){
	if(!ptr)return;

	spinlock_acquire(kernel->kheap.lock);
	
	kheap_segment *current_seg = (kheap_segment *)((uintptr_t)ptr - sizeof(kheap_segment));
	if(current_seg->magic != KHEAP_SEG_MAGIC_ALLOCATED){
		kdebugf("try to free not allocated kheap seg \n");
		spinlock_release(kernel->kheap.lock);
		return;
	}

	current_seg->magic = KHEAP_SEG_MAGIC_FREE;

	if(current_seg->next && current_seg->next->magic == KHEAP_SEG_MAGIC_FREE){
		//merge with next
		current_seg->lenght += current_seg->next->lenght + sizeof(kheap_segment);

		if(current_seg->next->next){
			current_seg->next->next->prev = current_seg;
		}
		current_seg->next = current_seg->next->next;
	}

	if(current_seg->prev && current_seg->prev->magic == KHEAP_SEG_MAGIC_FREE){
		//merge with prev
		current_seg->prev->lenght += current_seg->lenght + sizeof(kheap_segment);

		if(current_seg->next){
			current_seg->next->prev = current_seg->prev;
		}
		current_seg->prev->next = current_seg->next;
	}
	spinlock_release(kernel->kheap.lock);
}