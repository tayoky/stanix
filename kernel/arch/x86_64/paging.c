#include <kernel/kernel.h>
#include <kernel/paging.h>
#include <kernel/print.h>
#include <kernel/pmm.h>
#include <kernel/mmu.h> 
#include <kernel/string.h>

extern uint64_t p_kernel_start[];
extern uint64_t p_kernel_end[];
extern uint64_t p_kernel_text_end[];

#define SHARED_PML4_ENTRIES_COUNT 256
static uintptr_t shared_PML4_entries[SHARED_PML4_ENTRIES_COUNT];

static uint64_t mmu2paging_flags(long mmu_flags) {
	uint64_t flags = 0;
	if (mmu_flags & MMU_FLAG_PRESENT) {
		flags |= PAGING_FLAG_PRESENT;
	}
	if (mmu_flags & MMU_FLAG_WRITE) {
		flags |= PAGING_FLAG_WRITE;
	}
	if (!(mmu_flags & MMU_FLAG_EXEC)) {
		flags |= PAGING_FLAG_NO_EXE;
	}
	if (mmu_flags & MMU_FLAG_USER) {
		flags |= PAGING_FLAG_USER;
	}
	return flags;
}

static long paging2mmu_flags(uint64_t paging_flags) {
	long flags = 0;
	if (paging_flags & PAGING_FLAG_PRESENT) {
		flags |= MMU_FLAG_PRESENT | MMU_FLAG_READ;
	}
	if (paging_flags & PAGING_FLAG_WRITE) {
		flags |= MMU_FLAG_WRITE;
	}
	if (!(paging_flags & PAGING_FLAG_NO_EXE)) {
		flags |= MMU_FLAG_EXEC;
	}
	if (paging_flags & PAGING_FLAG_USER) {
		flags |= MMU_FLAG_USER;
	}
	if (paging_flags & PAGING_FLAG_ACCESS) {
		flags |= MMU_FLAG_ACCESS;
	}
	if (paging_flags & PAGING_FLAG_DIRTY) {
		flags |= MMU_FLAG_DIRTY;
	}
	return flags;
}

addrspace_t mmu_get_addr_space() {
	uint64_t cr3;
	asm("mov %%cr3, %%rax" : "=a" (cr3));
	return (addrspace_t)(cr3 + kernel->hhdm);
}

void mmu_set_addr_space(addrspace_t new_addrspace) {
	asm volatile ("movq %0, %%cr3" : : "r" ((uintptr_t)new_addrspace - kernel->hhdm));
}

void init_mmu(void) {
	kstatusf("init paging... ");

	// init the shared PML4 entries
	// the shared PML4 entries mapping are conserved across all address space
	// for kernel module kheap , ...
	for (int i = 0; i < SHARED_PML4_ENTRIES_COUNT; i++) {
		shared_PML4_entries[i] = pmm_allocate_page() | PAGING_FLAG_RW_CPL0;
		memset((void *)((shared_PML4_entries[i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm), 0, PAGE_SIZE);
	}

	addrspace_t PML4 = mmu_create_addr_space();

	// map kernel in it
	mmu_map_kernel(PML4);
	mmu_map_hhdm(PML4);
	mmu_set_addr_space(PML4);
	
	kok();
}

addrspace_t mmu_create_addr_space() {
	uint64_t *PML4 = (uint64_t *)(pmm_allocate_page() + kernel->hhdm);

	// Set all entry as 0
	memset(PML4, 0, PAGE_SIZE);

	//copy the shared PML4 entries
	memcpy(PML4 + (512 - SHARED_PML4_ENTRIES_COUNT), shared_PML4_entries, sizeof(uint64_t) * SHARED_PML4_ENTRIES_COUNT);

	kdebugf("create addrspace %p\n", PML4);
	return PML4;
}

void mmu_delete_addr_space(addrspace_t PML4) {
	//recusively free everythings
	//EXCEPT THE SHARED PML4 entries
	kdebugf("free addrspace %p\n", PML4);
	for (uint16_t PML4i = 0; PML4i < (512 - SHARED_PML4_ENTRIES_COUNT); PML4i++) {
		if (!(PML4[PML4i] & 1))continue;

		uint64_t *PDP = (uint64_t *)((PML4[PML4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
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

	pmm_free_page((uintptr_t)PML4 - kernel->hhdm);
}

uintptr_t mmu_virt2phys(void *address) {
	// just wrap arround space_virt2phys
	return mmu_space_virt2phys(mmu_get_addr_space(), address);
}

uintptr_t mmu_space_virt2phys(addrspace_t PML4, void *address) {
	uint64_t PML4i= ((uint64_t)address >> 39) & 0x1FF;
	uint64_t PDPi  = ((uint64_t)address >> 30) & 0x1FF;
	uint64_t PDi   = ((uint64_t)address >> 21) & 0x1FF;
	uint64_t PTi   = ((uint64_t)address >> 12) & 0x1FF;

	if (!(PML4[PML4i] & 1)) {
		return PAGE_INVALID;
	}

	uint64_t *PDP = (uint64_t *)((PML4[PML4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if (!(PDP[PDPi] & 1)) {
		return PAGE_INVALID;
	}

	uint64_t *PD = (uint64_t *)((PDP[PDPi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if (!(PD[PDi] & 1)) {
		return PAGE_INVALID;
	}

	uint64_t *PT = (uint64_t *)((PD[PDi] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
	if (!(PT[PTi] & 1)) {
		return PAGE_INVALID;
	}
	return (PT[PTi] & PAGING_ENTRY_ADDRESS) + ((uint64_t)address & 0XFFF);
}

void mmu_map_page(addrspace_t PML4, uintptr_t physical_page, uintptr_t vaddr, long mmu_flags) {
	uint64_t flags = mmu2paging_flags(mmu_flags);
	uint64_t PML4i= ((uint64_t)vaddr >> 39) & 0x1FF;
	uint64_t PDPi  = ((uint64_t)vaddr >> 30) & 0x1FF;
	uint64_t PDi   = ((uint64_t)vaddr >> 21) & 0x1FF;
	uint64_t PTi   = ((uint64_t)vaddr >> 12) & 0x1FF;

	if (!(PML4[PML4i] & 1)) {
		PML4[PML4i] = pmm_allocate_page() | PAGING_FLAG_RW_CPL3;
		memset((uint64_t *)((PML4[PML4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm), 0, PAGE_SIZE);
	}

	uint64_t *PDP = (uint64_t *)((PML4[PML4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
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
	PT[PTi] = (physical_page & ~0xFFFUL) | flags;

	asm volatile("invlpg (%0)" ::"r" (vaddr) : "memory");
}

static uint64_t *mmu_get_entry(addrspace_t PML4, uintptr_t vaddr) {uint64_t PML4i= ((uint64_t)vaddr >> 39) & 0x1FF;
	uint64_t PDPi  = ((uint64_t)vaddr >> 30) & 0x1FF;
	uint64_t PDi   = ((uint64_t)vaddr >> 21) & 0x1FF;
	uint64_t PTi   = ((uint64_t)vaddr >> 12) & 0x1FF;

	if (!(PML4[PML4i] & 1)) {
		return NULL;
	}

	uint64_t *PDP = (uint64_t *)((PML4[PML4i] & PAGING_ENTRY_ADDRESS) + kernel->hhdm);
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

	return &PT[PTi];
}

long mmu_get_flags(addrspace_t PML4, uintptr_t vaddr) {
	uint64_t *entry = mmu_get_entry(PML4, vaddr);
	if (!entry) return 0;
	if (*entry & PAGING_FLAG_PRESENT) {
		return paging2mmu_flags(*entry);
	} else {
		return 0;
	}
}

int mmu_set_flags(addrspace_t PML4, uintptr_t vaddr, long flags) {
	uint64_t *entry = mmu_get_entry(PML4, vaddr);
	if (!entry) return -EFAULT;
	uint64_t paging_flags = mmu2paging_flags(flags);
	*entry = (*entry & PAGING_ENTRY_ADDRESS) | paging_flags;
	return 0;
}

void mmu_unmap_page(addrspace_t PML4, uintptr_t vaddr) {
	uint64_t *entry = mmu_get_entry(PML4, vaddr);
	if (!entry) return;
	*entry = 0;

	if (mmu_get_addr_space() == PML4) asm volatile("invlpg (%0)" ::"r" (vaddr) : "memory");
}

void mmu_map_kernel(uint64_t *PML4) {
	uint64_t kernel_start      = (uint64_t) * (&p_kernel_start);
	uint64_t kernel_end        = (uint64_t) * (&p_kernel_end);
	uint64_t kernel_text_end   = (uint64_t) * (&p_kernel_text_end);

	uint64_t kernel_size = PAGE_DIV_UP(kernel_end - kernel_start);
	uint64_t phys_page = PAGE_ALIGN_DOWN(kernel->kernel_address->physical_base);
	uint64_t virt_page = PAGE_ALIGN_DOWN(kernel->kernel_address->virtual_base);

	for (uint64_t i = 0; i < kernel_size; i++) {
		long flags = MMU_FLAG_READ | MMU_FLAG_PRESENT | MMU_FLAG_GLOBAL;
		if (virt_page < kernel_text_end) {
			flags |= MMU_FLAG_EXEC;
		} else {
			flags |= MMU_FLAG_WRITE;
		}
			
		mmu_map_page(PML4, phys_page, virt_page, flags);
		phys_page += PAGE_SIZE;
		virt_page += PAGE_SIZE;
	}
}

void mmu_map_hhdm(uint64_t *PML4) {
	for (uint64_t index = 0; index < kernel->memmap->entry_count; index++) {
		uint64_t type = kernel->memmap->entries[index]->type;
		if (type == LIMINE_MEMMAP_ACPI_NVS ||
			type == LIMINE_MEMMAP_ACPI_RECLAIMABLE ||
			type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
			type == LIMINE_MEMMAP_FRAMEBUFFER ||
			type == LIMINE_MEMMAP_KERNEL_AND_MODULES ||
			type == LIMINE_MEMMAP_USABLE
			) {
			// map all the section
			long flags = MMU_FLAG_READ | MMU_FLAG_WRITE | MMU_FLAG_PRESENT | MMU_FLAG_GLOBAL;
			if (type == LIMINE_MEMMAP_FRAMEBUFFER) {
				// TODO : write combine
				//flags |= PAGING_FLAG_WRITE_COMBINE;
			}
			uintptr_t phys_page = PAGE_ALIGN_DOWN(kernel->memmap->entries[index]->base);
			uintptr_t end = PAGE_ALIGN_UP(kernel->memmap->entries[index]->base + kernel->memmap->entries[index]->length);
			uintptr_t virt_page = PAGE_ALIGN_DOWN(kernel->memmap->entries[index]->base + kernel->hhdm);
			while (phys_page < end) {
				mmu_map_page(PML4, phys_page, virt_page, flags);
				virt_page += PAGE_SIZE;
				phys_page += PAGE_SIZE;
			}

		}
	}

}
