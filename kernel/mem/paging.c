#include "kernel.h"
#include "paging.h"
#include "print.h"
#include "bitmap.h"

uint64_t *init_PMLT4(kernel_table *kernel){
	//allocate place for the PMLT4
	uint64_t *PMLT4 = PAGE_SIZE * allocate_page(&kernel->bitmap);

	//Set all entry as 0
	for (uint16_t i = 0; i < 512; i++)
	{
		PMLT4[i] = 0;
	}
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

void map_page();

