#include "kernel.h"
#include "paging.h"
#include "print.h"
#include "bitmap.h"
#include "string.h"

extern uint64_t p_kernel_start[];
extern uint64_t p_kernel_end[];

uint64_t *init_PMLT4(kernel_table *kernel){
	//allocate place for the PMLT4
	uint64_t *PMLT4 = PAGE_SIZE * allocate_page(&kernel->bitmap) + kernel->hhdm;
	kprintf("PMLT4 : 0x%lx\n",PMLT4);
	//Set all entry as 0
	memset(PMLT4,0,PAGE_SIZE);

	//map kernel in it
	map_kernel(kernel,PMLT4);
	//map the hhdm
	//map_hhdm(kernel,PMLT4);
	return PMLT4;
}

void *virt2phys(kernel_table *kernel,void *address){
	uint64_t PMLT4i= ((uint64_t)address >> 39) & 0x1FF;
	uint64_t PDPi  = ((uint64_t)address >> 30) & 0x1FF;
	uint64_t PDi   = ((uint64_t)address >> 21) & 0x1FF;
	uint64_t PTi   = ((uint64_t)address >> 12) & 0x1FF;

	uint64_t cr3;
	asm volatile("mov %%cr3, %%rax" : "=a" (cr3));

	uint64_t *PMLT4 = cr3 + kernel->hhdm;
	if(!PMLT4[PMLT4i] & 1){
		return NULL;
	}

	uint64_t *PDP = (PMLT4[PMLT4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm;
	if(!PDP[PDPi] & 1){
		return NULL;
	}

	uint64_t *PD = (PDP[PDPi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm;
	if(!PD[PDi] & 1){
		return NULL;
	}

	uint64_t *PT = (PD[PDi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm;
	if(!PT[PTi] & 1){
		return NULL;
	}

	return (void *) (PT[PTi] & PAGING_ENTRY_ADDRESS + ((uint64_t)address & 0XFFF));
}

void map_page(kernel_table *kernel,uint64_t *PMLT4,uint64_t physical_page,uint64_t virtual_page,uint8_t falgs){
	uint64_t PMLT4i= ((uint64_t)virtual_page >> 39) & 0x1FF;
	uint64_t PDPi  = ((uint64_t)virtual_page >> 30) & 0x1FF;
	uint64_t PDi   = ((uint64_t)virtual_page >> 21) & 0x1FF;
	uint64_t PTi   = ((uint64_t)virtual_page >> 12) & 0x1FF;

	if(!PMLT4[PMLT4i] & 1){
		PMLT4[PMLT4i] = PAGE_SIZE * allocate_page(&kernel->bitmap) + PAGING_FLAG_RW_CPL3;
		memset((PMLT4[PMLT4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm,0,PAGE_SIZE);
	}

	uint64_t *PDP = (PMLT4[PMLT4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm;
	if(!PDP[PDPi] & 1){
		PDP[PDi] = PAGE_SIZE * allocate_page(&kernel->bitmap) + PAGING_FLAG_RW_CPL3;
		memset((PDP[PDPi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm,0,PAGE_SIZE);
	}

	uint64_t *PD = (PDP[PDPi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm;
	if(!PD[PDi] & 1){
		PD[PDi] = PAGE_SIZE * allocate_page(&kernel->bitmap) + PAGING_FLAG_RW_CPL3;
		memset((PD[PDi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm,0,PAGE_SIZE);
	}

	uint64_t *PT = (PD[PDi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm;
	if(!PT[PTi] & 1){
		PT[PTi] = PAGE_SIZE * physical_page + falgs;
	}
}

void unmap_page(kernel_table *kernel,uint64_t *PMLT4,uint64_t virtual_page){
	uint64_t PMLT4i= ((uint64_t)virtual_page >> 39) & 0x1FF;
	uint64_t PDPi  = ((uint64_t)virtual_page >> 30) & 0x1FF;
	uint64_t PDi   = ((uint64_t)virtual_page >> 21) & 0x1FF;
	uint64_t PTi   = ((uint64_t)virtual_page >> 12) & 0x1FF;

	if(!PMLT4[PMLT4i] & 1){
		return;
	}

	uint64_t *PDP = (PMLT4[PMLT4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm;
	if(!PDP[PDPi] & 1){
		return;
	}

	uint64_t *PD = (PDP[PDPi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm;
	if(!PD[PDi] & 1){
		return;
	}

	uint64_t *PT = (PD[PDi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm;
	if(!PT[PTi] & 1){
		return;
	}

	PT[PTi] = 0;
}
///
void map_kernel(kernel_table *kernel,uint64_t *PMLT4){
	uint64_t kernel_start = *(&p_kernel_start);
	uint64_t kernel_end   = *(&p_kernel_end);
	uint64_t kernel_size = PAGE_ALIGN_UP(kernel_end - kernel_start);
	uint64_t phys_page = kernel->kernel_address->physical_base / PAGE_SIZE;
	uint64_t virt_page = kernel->kernel_address->virtual_base / PAGE_SIZE;
	kdebugf("start : 0x%lx\n",kernel_start);
	kdebugf("end   : 0x%lx\n",kernel_end);
	kdebugf("kernel size : 0x%lx\n",kernel_size);
	for (uint64_t i = 0; i < kernel_size; i++){
		map_page(kernel,PMLT4,phys_page,virt_page,PAGING_FLAG_RW_CPL0);
		phys_page++;
		virt_page++;
	}
}

void map_hhdm(kernel_table *kernel,uint64_t *PMLT4){
	for (uint64_t index = 0; index < kernel->memmap->entry_count; index++){
		uint64_t type = kernel->memmap->entries[index]->type;
		if(type == LIMINE_MEMMAP_ACPI_NVS ||
			type == LIMINE_MEMMAP_ACPI_RECLAIMABLE ||
			type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
			type == LIMINE_MEMMAP_FRAMEBUFFER ||
			type == LIMINE_MEMMAP_KERNEL_AND_MODULES ||
			type == LIMINE_MEMMAP_USABLE){
				//map all the section
				uint64_t section_size = PAGE_ALIGN_UP(kernel->memmap->entries[index]->base);
				uint64_t phys_page = kernel->memmap->entries[index]->base / PAGE_SIZE;
				uint64_t virt_page = (kernel->memmap->entries[index]->base + kernel->hhdm) / PAGE_SIZE;
				for (uint64_t i = 0; i < section_size; i++){
					kprintf("base : 0x%lx\n",virt_page);
					map_page(kernel,PMLT4,phys_page,virt_page,PAGING_FLAG_RW_CPL0);
					virt_page++;
					phys_page++;
				}
				
			}
	}
	
}