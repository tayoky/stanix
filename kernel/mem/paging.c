#include "kernel.h"
#include "paging.h"
#include "print.h"
#include "bitmap.h"
#include "string.h"

extern uint64_t p_kernel_start[];
extern uint64_t p_kernel_end[];

void init_paging(void){
	kstatus("init paging... ");
	uint64_t *PMLT4 = init_PMLT4(kernel);
	uint64_t cr3 = (uint64_t)PMLT4 - kernel->hhdm;
	asm volatile ("movq %0, %%cr3" : :  "r" (cr3) );
	kok();
}

uint64_t *init_PMLT4(kernel_table *kernel){
	//allocate place for the PMLT4
	uint64_t *PMLT4 = (uint64_t *)(PAGE_SIZE * allocate_page(&kernel->bitmap) + kernel->hhdm);

	//Set all entry as 0
	memset(PMLT4,0,PAGE_SIZE);

	//map kernel in it
	map_kernel(PMLT4);

	//map the hhdm
	map_hhdm(PMLT4);

	//map the kernel heap
	map_kheap(PMLT4);

	return PMLT4;
}

void delete_PMLT4(uint64_t *PMLT4){
	//first free the kernel stack
	for (size_t cur = KERNEL_STACK_BOTTOM; cur < KERNEL_STACK_TOP; cur+= PAGE_SIZE){
		free_page(&kernel->bitmap,(uintptr_t)PMLT4_virt2phys(PMLT4,(void *)cur)/PAGE_SIZE);
	}
	

	//recusively free everythings
	//EXCEPT THE KHEAP TABLES!!!

	for (uint16_t PMLT4i = 0; PMLT4i < 512; PMLT4i++){
		if(!PMLT4[PMLT4i] & 1)continue;

		//special check for the kheap
		if(PMLT4i == kernel->kheap.PMLT4i)continue;

		uint64_t *PDP = (uint64_t *)((PMLT4[PMLT4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);

		for (uint16_t PDPi = 0; PDPi < 512; PDPi++){
			if(!PDP[PDPi] & 1)continue;
			uint64_t *PD = (uint64_t *)((PDP[PDPi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);

			for (uint16_t PDi = 0; PDi < 512; PDi++){
				if(!PD[PDi] & 1)continue;
				uint64_t *PT = (uint64_t *)((PMLT4[PDi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
				
				free_page(&kernel->bitmap,((uint64_t)PT-kernel->hhdm)/PAGE_SIZE);
			}

			free_page(&kernel->bitmap,((uint64_t)PD-kernel->hhdm)/PAGE_SIZE);
		}

		free_page(&kernel->bitmap,((uint64_t)PDP-kernel->hhdm)/PAGE_SIZE);
	}
	

	free_page(&kernel->bitmap,((uint64_t)PMLT4-kernel->hhdm)/PAGE_SIZE);
}

void *virt2phys(void *address){
	//find the PMLT4 of the current address space
	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r" (cr3));
	uint64_t *PMLT4 = (uint64_t *)(cr3 + kernel->hhdm);

	//and then just wrap arround PMLT4_virt2phys
	return PMLT4_virt2phys(PMLT4,address);
}

void *PMLT4_virt2phys(uint64_t *PMLT4, void *address){
	uint64_t PMLT4i= ((uint64_t)address >> 39) & 0x1FF;
	uint64_t PDPi  = ((uint64_t)address >> 30) & 0x1FF;
	uint64_t PDi   = ((uint64_t)address >> 21) & 0x1FF;
	uint64_t PTi   = ((uint64_t)address >> 12) & 0x1FF;
	
	if(!PMLT4[PMLT4i] & 1){
		return NULL;
	}

	uint64_t *PDP = (uint64_t *)((PMLT4[PMLT4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if(!PDP[PDPi] & 1){
		return NULL;
	}

	uint64_t *PD = (uint64_t *)((PDP[PDPi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if(!PD[PDi] & 1){
		return NULL;
	}

	uint64_t *PT = (uint64_t *)((PD[PDi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if(!PT[PTi] & 1){
		return NULL;
	}

	return (void *) ((PT[PTi] & PAGING_ENTRY_ADDRESS) + ((uint64_t)address & 0XFFF));
}

void map_page(uint64_t *PMLT4,uint64_t physical_page,uint64_t virtual_page,uint64_t falgs){
	uint64_t PMLT4i= ((uint64_t)virtual_page >> 27) & 0x1FF;
	uint64_t PDPi  = ((uint64_t)virtual_page >> 18) & 0x1FF;
	uint64_t PDi   = ((uint64_t)virtual_page >> 9) & 0x1FF;
	uint64_t PTi   = ((uint64_t)virtual_page >> 0) & 0x1FF;
	
	if(!PMLT4[PMLT4i] & 1){
		PMLT4[PMLT4i] = PAGE_SIZE * allocate_page(&kernel->bitmap) + PAGING_FLAG_RW_CPL3;
		memset((uint64_t *)((PMLT4[PMLT4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm),0,PAGE_SIZE);
	}

	uint64_t *PDP = (uint64_t *)((PMLT4[PMLT4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if(!PDP[PDPi] & 1){
		PDP[PDPi] = PAGE_SIZE * allocate_page(&kernel->bitmap) + PAGING_FLAG_RW_CPL3;
		memset((uint64_t *)((PDP[PDPi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm),0,PAGE_SIZE);
	}

	uint64_t *PD = (uint64_t *)((PDP[PDPi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if(!PD[PDi] & 1){
		PD[PDi] = PAGE_SIZE * allocate_page(&kernel->bitmap) + PAGING_FLAG_RW_CPL3;
		memset((uint64_t *)((PD[PDi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm),0,PAGE_SIZE);
	}

	uint64_t *PT = (uint64_t *)((PD[PDi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if(!PT[PTi] & 1){
		PT[PTi] = (PAGE_SIZE * physical_page) | falgs;
	}
}

void unmap_page(uint64_t *PMLT4,uint64_t virtual_page){
	uint64_t PMLT4i= ((uint64_t)virtual_page >> 27) & 0x1FF;
	uint64_t PDPi  = ((uint64_t)virtual_page >> 18) & 0x1FF;
	uint64_t PDi   = ((uint64_t)virtual_page >> 9) & 0x1FF;
	uint64_t PTi   = ((uint64_t)virtual_page >> 0) & 0x1FF;

	if(!PMLT4[PMLT4i] & 1){
		return;
	}

	uint64_t *PDP = (uint64_t *)((PMLT4[PMLT4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if(!PDP[PDPi] & 1){
		return;
	}

	uint64_t *PD = (uint64_t *)((PDP[PDPi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if(!PD[PDi] & 1){
		return;
	}

	uint64_t *PT = (uint64_t *)((PD[PDi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if(!PT[PTi] & 1){
		return;
	}

	PT[PTi] = 0;

	//if there are not more mapped entries in a table we can delete it
	for (uint16_t i = 0; i < 512; i++){
		if(PT[i] & 1){
			return;
		}
	}
	free_page(&kernel->bitmap,((uint64_t)PT-kernel->hhdm)/PAGE_SIZE);
	PD[PDi] = 0;
	
	for (uint16_t i = 0; i < 512; i++){
		if(PD[i] & 1){
			return;
		}
	}
	free_page(&kernel->bitmap,((uint64_t)PD-kernel->hhdm)/PAGE_SIZE);
	PDP[PDPi] = 0;

	for (uint16_t i = 0; i < 512; i++){
		if(PDP[i] & 1){
			return;
		}
	}
	free_page(&kernel->bitmap,((uint64_t)PDP-kernel->hhdm)/PAGE_SIZE);
	PMLT4[PMLT4i] = 0;
}
///
void map_kernel(uint64_t *PMLT4){
	uint64_t kernel_start = (uint64_t)*(&p_kernel_start);
	uint64_t kernel_end   = (uint64_t)*(&p_kernel_end);
	uint64_t kernel_size = PAGE_DIV_UP(kernel_end - kernel_start);
	uint64_t phys_page = kernel->kernel_address->physical_base / PAGE_SIZE;
	uint64_t virt_page = kernel->kernel_address->virtual_base / PAGE_SIZE;

	for (uint64_t i = 0; i < kernel_size; i++){
		map_page(PMLT4,phys_page,virt_page,PAGING_FLAG_RW_CPL0);
		phys_page++;
		virt_page++;
	}

	//map the stack
	uint64_t kernel_stack_page = KERNEL_STACK_SIZE / PAGE_SIZE;
	virt_page = KERNEL_STACK_BOTTOM / PAGE_SIZE;
	for (size_t i = 0; i < kernel_stack_page; i++){
		map_page(PMLT4,allocate_page(&kernel->bitmap),virt_page,PAGING_FLAG_NO_EXE | PAGING_FLAG_RW_CPL0);
		virt_page++;
	}
}

void map_hhdm(uint64_t *PMLT4){
	for (uint64_t index = 0; index < kernel->memmap->entry_count; index++){
		uint64_t type = kernel->memmap->entries[index]->type;
		if(type == LIMINE_MEMMAP_ACPI_NVS ||
			type == LIMINE_MEMMAP_ACPI_RECLAIMABLE ||
			type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
			type == LIMINE_MEMMAP_FRAMEBUFFER ||
			type == LIMINE_MEMMAP_KERNEL_AND_MODULES ||
			type == LIMINE_MEMMAP_USABLE
			){
				//map all the section
				uint64_t section_size = PAGE_DIV_UP(kernel->memmap->entries[index]->length);
				uint64_t phys_page = kernel->memmap->entries[index]->base / PAGE_SIZE;
				uint64_t virt_page = (kernel->memmap->entries[index]->base + kernel->hhdm) / PAGE_SIZE;
				for (uint64_t i = 0; i < section_size; i++){
					map_page(PMLT4,phys_page,virt_page,PAGING_FLAG_RW_CPL0);
					virt_page++;
					phys_page++;
				}
				
			}
	}
	
}

void map_kheap(uint64_t *PMLT4){
	PMLT4[kernel->kheap.PMLT4i] = (uint64_t)kernel->kheap.PDP | PAGING_FLAG_RW_CPL0;
}