#include <kernel/kernel.h>
#include <kernel/paging.h>
#include <kernel/print.h>
#include <kernel/pmm.h>
#include <kernel/string.h>

extern uint64_t p_kernel_start[];
extern uint64_t p_kernel_end[];
extern uint64_t p_kernel_text_end[];

static uintptr_t higger_PDP[32];

addrspace_t get_addr_space() {
	uint64_t cr3;
	asm("mov %%cr3, %%rax" : "=a" (cr3));
	return (addrspace_t)(cr3 + kernel->hhdm);
}

void set_addr_space(addrspace_t new_addrspace) {
	asm volatile ("movq %0, %%cr3" : : "r" ((uintptr_t)new_addrspace - kernel->hhdm));
}

void init_paging(void) {
	kstatusf("init paging... ");

	//init the 32 higher PDP
	//the 32 higher PDP mapping are conserved across all address space
	//for kernel module kheap , ...
	for (int i = 0; i < 32; i++) {
		higger_PDP[i] = pmm_allocate_page() | PAGING_FLAG_RW_CPL0;
		memset((void *)((higger_PDP[i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm), 0, PAGE_SIZE);
	}

	addrspace_t PMLT4 = create_addr_space();

	//map kernel in it
	map_kernel(PMLT4);
	map_PMM_info(PMLT4);

	set_addr_space(PMLT4);

	kok();
}

addrspace_t create_addr_space() {
	//allocate place for the PMLT4
	uint64_t *PMLT4 = (uint64_t *)(pmm_allocate_page() + kernel->hhdm);

	//Set all entry as 0
	memset(PMLT4, 0, PAGE_SIZE);

	//copy the 32 higher PDP
	memcpy(PMLT4 + (512 - 32), higger_PDP, sizeof(uint64_t) * 32);

	//map the hhdm
	map_hhdm(PMLT4);

	kdebugf("create addrspace %p\n", PMLT4);
	return PMLT4;
}

void delete_addr_space(addrspace_t PMLT4) {
	//recusively free everythings
	//EXCEPT THE HIGHER PDP
	kdebugf("free addrspace %p\n", PMLT4);
	for (uint16_t PMLT4i = 0; PMLT4i < (512 - 32); PMLT4i++) {
		if (!(PMLT4[PMLT4i] & 1))continue;

		uint64_t *PDP = (uint64_t *)((PMLT4[PMLT4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
		for (uint16_t PDPi = 0; PDPi < 512; PDPi++) {
			if (!(PDP[PDPi] & 1))continue;
			uint64_t *PD = (uint64_t *)((PDP[PDPi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);

			for (uint16_t PDi = 0; PDi < 512; PDi++) {
				if (!(PD[PDi] & 1))continue;
				uint64_t *PT = (uint64_t *)((PD[PDi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);

				pmm_free_page((uintptr_t)PT - kernel->hhdm);
			}

			pmm_free_page((uintptr_t)PD - kernel->hhdm);
		}

		pmm_free_page((uintptr_t)PDP - kernel->hhdm);
	}

	pmm_free_page((uintptr_t)PMLT4 - kernel->hhdm);
}

void *virt2phys(void *address) {
	//ust wrap arround space_virt2phys
	return space_virt2phys(get_addr_space(), address);
}

void *space_virt2phys(addrspace_t PMLT4, void *address) {
	uint64_t PMLT4i= ((uint64_t)address >> 39) & 0x1FF;
	uint64_t PDPi  = ((uint64_t)address >> 30) & 0x1FF;
	uint64_t PDi   = ((uint64_t)address >> 21) & 0x1FF;
	uint64_t PTi   = ((uint64_t)address >> 12) & 0x1FF;

	if (!(PMLT4[PMLT4i] & 1)) {
		return NULL;
	}

	uint64_t *PDP = (uint64_t *)((PMLT4[PMLT4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if (!(PDP[PDPi] & 1)) {
		return NULL;
	}

	uint64_t *PD = (uint64_t *)((PDP[PDPi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if (!(PD[PDi] & 1)) {
		return NULL;
	}

	uint64_t *PT = (uint64_t *)((PD[PDi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if (!(PT[PTi] & 1)) {
		return NULL;
	}
	return (void *)((PT[PTi] & PAGING_ENTRY_ADDRESS) + ((uint64_t)address & 0XFFF));
}

void map_page(addrspace_t PMLT4, uintptr_t physical_page, uintptr_t virtual_addr, uint64_t falgs) {
	uint64_t PMLT4i= ((uint64_t)virtual_addr >> 39) & 0x1FF;
	uint64_t PDPi  = ((uint64_t)virtual_addr >> 30) & 0x1FF;
	uint64_t PDi   = ((uint64_t)virtual_addr >> 21) & 0x1FF;
	uint64_t PTi   = ((uint64_t)virtual_addr >> 12) & 0x1FF;

	if (!(PMLT4[PMLT4i] & 1)) {
		PMLT4[PMLT4i] = pmm_allocate_page() | PAGING_FLAG_RW_CPL3;
		memset((uint64_t *)((PMLT4[PMLT4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm), 0, PAGE_SIZE);
	}

	uint64_t *PDP = (uint64_t *)((PMLT4[PMLT4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if (!(PDP[PDPi] & 1)) {
		PDP[PDPi] = pmm_allocate_page() | PAGING_FLAG_RW_CPL3;
		memset((uint64_t *)((PDP[PDPi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm), 0, PAGE_SIZE);
	}

	uint64_t *PD = (uint64_t *)((PDP[PDPi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if (!(PD[PDi] & 1)) {
		PD[PDi] = pmm_allocate_page() | PAGING_FLAG_RW_CPL3;
		memset((uint64_t *)((PD[PDi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm), 0, PAGE_SIZE);
	}

	uint64_t *PT = (uint64_t *)((PD[PDi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	PT[PTi] = (physical_page & ~0xFFFUL) | falgs;

	asm volatile("invlpg (%0)" ::"r" (virtual_addr) : "memory");
}

void unmap_page(addrspace_t PMLT4, uintptr_t virtual_addr) {
	uint64_t PMLT4i= ((uint64_t)virtual_addr >> 39) & 0x1FF;
	uint64_t PDPi  = ((uint64_t)virtual_addr >> 30) & 0x1FF;
	uint64_t PDi   = ((uint64_t)virtual_addr >> 21) & 0x1FF;
	uint64_t PTi   = ((uint64_t)virtual_addr >> 12) & 0x1FF;

	if (!(PMLT4[PMLT4i] & 1)) {
		return;
	}

	uint64_t *PDP = (uint64_t *)((PMLT4[PMLT4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if (!(PDP[PDPi] & 1)) {
		return;
	}

	uint64_t *PD = (uint64_t *)((PDP[PDPi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if (!(PD[PDi] & 1)) {
		return;
	}

	uint64_t *PT = (uint64_t *)((PD[PDi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if (!(PT[PTi] & 1)) {
		return;
	}

	PT[PTi] = 0;

	if (get_addr_space() == PMLT4)asm volatile("invlpg (%0)" ::"r" (virtual_addr) : "memory");

	//if there are not more mapped entries in a table we can delete it
	for (uint16_t i = 0; i < 512; i++) {
		if (PT[i] & 1) {
			return;
		}
	}
	pmm_free_page((uintptr_t)PT - kernel->hhdm);
	PD[PDi] = 0;

	for (uint16_t i = 0; i < 512; i++) {
		if (PD[i] & 1) {
			return;
		}
	}
	pmm_free_page((uintptr_t)PD - kernel->hhdm);
	PDP[PDPi] = 0;
}

void map_kernel(uint64_t *PMLT4) {
	uint64_t kernel_start      = (uint64_t) * (&p_kernel_start);
	uint64_t kernel_end        = (uint64_t) * (&p_kernel_end);
	uint64_t kernel_text_end   = (uint64_t) * (&p_kernel_text_end);

	uint64_t kernel_size = PAGE_DIV_UP(kernel_end - kernel_start);
	uint64_t phys_page = PAGE_ALIGN_DOWN(kernel->kernel_address->physical_base);
	uint64_t virt_page = PAGE_ALIGN_DOWN(kernel->kernel_address->virtual_base);

	for (uint64_t i = 0; i < kernel_size; i++) {
		uint64_t flags = PAGING_FLAG_READONLY_CPL0;
		if (virt_page >= kernel_text_end) {
			flags |= PAGING_FLAG_RW_CPL0 | PAGING_FLAG_NO_EXE;
		}
		map_page(PMLT4, phys_page, virt_page, PAGING_FLAG_RW_CPL0);
		phys_page += PAGE_SIZE;
		virt_page += PAGE_SIZE;
	}
}

void map_hhdm(uint64_t *PMLT4) {
	for (uint64_t index = 0; index < kernel->memmap->entry_count; index++) {
		uint64_t type = kernel->memmap->entries[index]->type;
		if (type == LIMINE_MEMMAP_ACPI_NVS ||
			type == LIMINE_MEMMAP_ACPI_RECLAIMABLE ||
			type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
			type == LIMINE_MEMMAP_FRAMEBUFFER ||
			type == LIMINE_MEMMAP_KERNEL_AND_MODULES ||
			type == LIMINE_MEMMAP_USABLE
			) {
			//map all the section
			uint64_t flags = PAGING_FLAG_RW_CPL0;
			if (type == LIMINE_MEMMAP_FRAMEBUFFER) {
				flags |= PAGING_FLAG_WRITE_COMBINE;
			}
			uintptr_t phys_page = PAGE_ALIGN_DOWN(kernel->memmap->entries[index]->base);
			uintptr_t end = PAGE_ALIGN_UP(kernel->memmap->entries[index]->base + kernel->memmap->entries[index]->length);
			uintptr_t virt_page = PAGE_ALIGN_DOWN(kernel->memmap->entries[index]->base + kernel->hhdm);
			while (phys_page < end) {
				map_page(PMLT4, phys_page, virt_page, flags);
				virt_page += PAGE_SIZE;
				phys_page += PAGE_SIZE;
			}

		}
	}

}