#include "paging.h"
#include "kheap.h"
#include "print.h"


void init_kheap(kernel_table *kernel){
	kstatus("init kheap...");
	kernel->kheap.start = PAGE_ALIGN_DOWN(0xFFFFFFFF90000000);
	//get cr3
	uint64_t cr3;
	asm("mov %%cr3, %%rax" : "=a" (cr3));
	uint64_t *PMLT4 = cr3 + kernel->hhdm;
	map_page(kernel,PMLT4,allocate_page(&kernel->bitmap),kernel->kheap.start/PAGE_SIZE,PAGING_FLAG_RW_CPL0 | PAGING_FLAG_NO_EXE);
	kernel->kheap.lenght = PAGE_SIZE;
	kok();
}

void change_kheap_size(kernel_table *kernel,int64_t offset){
	if(!offset)return;
	int64_t offset_page = offset / PAGE_SIZE;

	//get the PMLT4
	uint64_t cr3;
	asm("mov %%cr3, %%rax" : "=a" (cr3));
	uint64_t *PMLT4 = cr3 + kernel->hhdm;

	if(offset < 0){
		//make kheap smaller
		for (uint64_t i = 0; i > offset_page; i--){
			uint64_t virt_page = (kernel->kheap.start + kernel->kheap.lenght)/PAGE_SIZE + i;
			uint64_t phys_page = (uint64_t)virt2phys(kernel,virt_page*PAGE_SIZE) / PAGE_SIZE;
			unmap_page(kernel,PMLT4,virt_page);
			free_page(&kernel->bitmap,phys_page);
		}
	} else {
		//make kheap bigger
		for (uint64_t i = 0; i < offset_page; i++){
			map_page(kernel,PMLT4,allocate_page(&kernel->bitmap),(kernel->kheap.start+kernel->kheap.lenght)/PAGE_SIZE+i,PAGING_FLAG_RW_CPL0 | PAGING_FLAG_NO_EXE);
		}
	}
	kernel->kheap.lenght += offset_page * PAGE_SIZE;
}