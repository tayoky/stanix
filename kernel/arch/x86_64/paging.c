#include <kernel/bootinfo.h>
#include <kernel/kernel.h>
#include <kernel/mmu.h>
#include <kernel/paging.h>
#include <kernel/pmm.h>
#include <kernel/rcu.h>
#include <kernel/print.h>
#include <kernel/string.h>

extern uint64_t p_kernel_start[];
extern uint64_t p_kernel_end[];
extern uint64_t p_kernel_text_end[];

#define SHARED_PML4_ENTRIES_COUNT 256
static uintptr_t shared_PML4_entries[SHARED_PML4_ENTRIES_COUNT];
uintptr_t hhdm_base;

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
	if (mmu_flags & MMU_FLAG_GLOBAL) {
		flags |= PAGING_FLAG_GLOBAL;
	}
	if (mmu_flags & MMU_FLAG_WRITE_COMBINE) {
		flags |= PAGING_FLAG_PAT;
	}
	if (mmu_flags & MMU_FLAG_UNCACHED) {
		flags |= PAGING_FLAG_PCD;
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
	if (paging_flags & PAGING_FLAG_PAT) {
		flags |= MMU_FLAG_WRITE_COMBINE;
	}
	if (paging_flags & PAGING_FLAG_PCD) {
		flags |= MMU_FLAG_UNCACHED;
	}
	return flags;
}

static void mmu_invalidate(uintptr_t vaddr) {
	asm volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
}

mmu_space_t mmu_get_addr_space() {
	uint64_t cr3;
	asm("mov %%cr3, %%rax" : "=a"(cr3));
	return (mmu_space_t)mmu_phys2virt(cr3);
}

void mmu_set_addr_space(mmu_space_t new_addrspace) {
	asm volatile("movq %0, %%cr3" : : "r"(mmu_hhdm2phys(new_addrspace)));
}

void init_mmu(void) {
	kstatusf("init paging... ");

	// init the shared PML4 entries
	// the shared PML4 entries mapping are conserved across all address space
	// for kernel module kheap , ...
	for (int i = 0; i < SHARED_PML4_ENTRIES_COUNT; i++) {
		shared_PML4_entries[i] = pmm_allocate_page() | PAGING_FLAG_RW_CPL0;
		memset(mmu_phys2virt(shared_PML4_entries[i] & PAGING_ENTRY_ADDRESS), 0, PAGE_SIZE);
	}

	mmu_space_t PML4 = mmu_space_init();

	// map kernel in it
	mmu_map_kernel(PML4);
	mmu_map_hhdm(PML4);
	mmu_set_addr_space(PML4);

	uint32_t low;
	uint32_t high;
	rdmsr(IA32_PAT_MSR, &low, &high);
	high &= ~0xff;
	high |= PAGING_PAT_WC;
	wrmsr(IA32_PAT_MSR, low, high);

	kok();
}

mmu_space_t mmu_space_init(void) {
	uintptr_t page = pmm_allocate_page();
	if (!page) return NULL;

	uint64_t *PML4 = mmu_phys2virt(pmm_allocate_page());

	// set every entry to 0
	memset(PML4, 0, PAGE_SIZE);

	// copy the shared PML4 entries
	memcpy(PML4 + (512 - SHARED_PML4_ENTRIES_COUNT), shared_PML4_entries, sizeof(uint64_t) * SHARED_PML4_ENTRIES_COUNT);

	kdebugf("create addrspace %p\n", PML4);
	return PML4;
}

void mmu_space_destroy(mmu_space_t PML4) {
	// recusively free everythings
	// EXCEPT THE SHARED PML4 entries
	kdebugf("free addrspace %p\n", PML4);
	for (uint16_t PML4i = 0; PML4i < (512 - SHARED_PML4_ENTRIES_COUNT); PML4i++) {
		if (!(PML4[PML4i] & 1)) continue;

		uint64_t *PDP = mmu_phys2virt(PML4[PML4i] & PAGING_ENTRY_ADDRESS);
		for (uint16_t PDPi = 0; PDPi < 512; PDPi++) {
			if (!(PDP[PDPi] & 1)) continue;
			uint64_t *PD = mmu_phys2virt(PDP[PDPi] & PAGING_ENTRY_ADDRESS);

			for (uint16_t PDi = 0; PDi < 512; PDi++) {
				if (!(PD[PDi] & 1)) continue;
				uint64_t *PT = mmu_phys2virt(PD[PDi] & PAGING_ENTRY_ADDRESS);

				pmm_release_page(mmu_hhdm2phys(PT));
			}

			pmm_release_page(mmu_hhdm2phys(PD));
		}

		pmm_release_page(mmu_hhdm2phys(PDP));
	}

	pmm_release_page(mmu_hhdm2phys(PML4));
}

void mmu_set_hhdm(uintptr_t hhdm) {
	hhdm_base = hhdm;
}

mmu_entry_t *mmu_create_entry_and_acquire_lock(mmu_space_t PML4, uintptr_t vaddr) {
	uint64_t PML4i = ((uint64_t)vaddr >> 39) & 0x1FF;
	uint64_t PDPi  = ((uint64_t)vaddr >> 30) & 0x1FF;
	uint64_t PDi   = ((uint64_t)vaddr >> 21) & 0x1FF;
	uint64_t PTi   = ((uint64_t)vaddr >> 12) & 0x1FF;

	pmm_acquire_page_lock(mmu_hhdm2phys(PML4));

	if (!(PML4[PML4i] & PAGING_FLAG_PRESENT)) {
		uintptr_t page = pmm_allocate_page();
		if (page == PAGE_INVALID) {
			pmm_release_page_lock(mmu_hhdm2phys(PML4));
			return NULL;
		}
		memset(mmu_phys2virt(page), 0, PAGE_SIZE);
		atomic_store(&PML4[PML4i], page | PAGING_FLAG_RW_CPL3);
	}

	mmu_entry_t *PDP = mmu_phys2virt(PML4[PML4i] & PAGING_ENTRY_ADDRESS);
	pmm_acquire_page_lock(mmu_hhdm2phys(PDP));
	pmm_release_page_lock(mmu_hhdm2phys(PML4));
	if (!(PDP[PDPi] & PAGING_FLAG_PRESENT)) {
		uintptr_t page = pmm_allocate_page();
		if (page == PAGE_INVALID) {
			pmm_release_page_lock(mmu_hhdm2phys(PDP));
			return NULL;
		}
		memset(mmu_phys2virt(page), 0, PAGE_SIZE);
		atomic_store(&PDP[PDPi], page | PAGING_FLAG_RW_CPL3);
	}

	mmu_entry_t *PD = mmu_phys2virt(PDP[PDPi] & PAGING_ENTRY_ADDRESS);
	pmm_acquire_page_lock(mmu_hhdm2phys(PD));
	pmm_release_page_lock(mmu_hhdm2phys(PDP));
	if (!(PD[PDi] & PAGING_FLAG_PRESENT)) {
		uintptr_t page = pmm_allocate_page();
		if (page == PAGE_INVALID) {
			pmm_release_page_lock(mmu_hhdm2phys(PD));
			return NULL;
		}
		memset(mmu_phys2virt(page), 0, PAGE_SIZE);
		atomic_store(&PD[PDi], page | PAGING_FLAG_RW_CPL3);
	}
	
	mmu_entry_t *PT = mmu_phys2virt(PD[PDi] & PAGING_ENTRY_ADDRESS);
	pmm_acquire_page_lock(mmu_hhdm2phys(PT));
	pmm_release_page_lock(mmu_hhdm2phys(PD));

	return &PT[PTi];
}

int mmu_map_pages(mmu_space_t PML4, uintptr_t paddr, uintptr_t vaddr, size_t size, long flags) {
	while (size > 0) {
		mmu_entry_t *entry = mmu_create_entry_and_acquire_lock(PML4, vaddr);
		if (entry) return -ENOMEM;
		mmu_entry_map(PML4, entry, paddr, flags);
		mmu_entry_release_lock(entry);
		if (mmu_get_addr_space() == PML4) mmu_invalidate(vaddr);
		paddr += PAGE_SIZE;
		vaddr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	return 0;
}

mmu_entry_t *mmu_get_entry(mmu_space_t PML4, uintptr_t vaddr) {
	uint64_t PML4i = ((uint64_t)vaddr >> 39) & 0x1FF;
	uint64_t PDPi  = ((uint64_t)vaddr >> 30) & 0x1FF;
	uint64_t PDi   = ((uint64_t)vaddr >> 21) & 0x1FF;
	uint64_t PTi   = ((uint64_t)vaddr >> 12) & 0x1FF;

	rcu_acquire_read();
	mmu_entry_val_t PML4_entry = atomic_load(&PML4[PML4i]);
	if (!(PML4_entry & PAGING_FLAG_PRESENT)) {
		rcu_release_read();
		return NULL;
	}

	mmu_entry_t *PDP = mmu_phys2virt(PML4_entry & PAGING_ENTRY_ADDRESS);
	mmu_entry_val_t PDP_entry = atomic_load(&PDP[PDPi]);
	if (!(PDP_entry & PAGING_FLAG_PRESENT)) {
		rcu_release_read();
		return NULL;
	}

	mmu_entry_t *PD = mmu_phys2virt(PDP_entry & PAGING_ENTRY_ADDRESS);
	mmu_entry_val_t PD_entry = atomic_load(&PD[PDi]);
	if (!(PD_entry & PAGING_FLAG_PRESENT)) {
		rcu_release_read();
		return NULL;
	}

	mmu_entry_t *PT = mmu_phys2virt(PD_entry & PAGING_ENTRY_ADDRESS);
	rcu_release_read();
	return &PT[PTi];
}

uintptr_t mmu_virt2phys(void *address) {
	// just wrap arround space_virt2phys
	return mmu_space_virt2phys(mmu_get_addr_space(), address);
}

uintptr_t mmu_space_virt2phys(mmu_space_t PML4, void *address) {
	rcu_acquire_read();
	mmu_entry_t *entry = mmu_get_entry(PML4, (uintptr_t)address);
	if (!entry) {
		rcu_release_read();
		return PAGE_INVALID;
	}

	mmu_entry_val_t entry_val = atomic_load(entry);
	if (entry_val & PAGING_FLAG_PRESENT) {
		uintptr_t phys = (entry_val & PAGING_ENTRY_ADDRESS) + ((uintptr_t)address & 0xfff);
		rcu_release_read();
		return phys;
	} else {
		rcu_release_read();
		return PAGE_INVALID;
	}
}

void mmu_entry_map(mmu_space_t PML4, mmu_entry_t *entry, uintptr_t paddr, long flags) {
	(void)PML4;
	uint64_t paging_flags = mmu2paging_flags(flags);

	atomic_store(entry, (paddr & ~0xFFFUL) | paging_flags);
}

long mmu_entry_unmap(mmu_space_t PML4, mmu_entry_t *entry, uintptr_t vaddr) {
	mmu_entry_val_t entry_val = atomic_exchange(entry, 0);

	if (entry_val & MMU_FLAG_PRESENT) {
		if (mmu_get_addr_space() == PML4) mmu_invalidate(vaddr);
		return paging2mmu_flags(entry_val);
	} else {
		return 0;
	}
}

long mmu_entry_get_flags(mmu_space_t PML4, mmu_entry_t *entry) {
	(void)PML4;
	mmu_entry_val_t value = atomic_load(entry);
	if (value & PAGING_FLAG_PRESENT) {
		long flags = paging2mmu_flags(value);
		return flags;
	} else {
		return 0;
	}
}
uintptr_t mmu_entry_get_page(mmu_space_t PML4, mmu_entry_t *entry) {
	(void)PML4;
	mmu_entry_val_t value = atomic_load(entry);
	if (value & PAGING_FLAG_PRESENT) {
		return value & PAGING_ENTRY_ADDRESS;
	} else {
		return 0;
	}
}

int mmu_entry_set_flags(mmu_space_t PML4, mmu_entry_t *entry, long flags) {
	(void)PML4;
	mmu_entry_val_t paging_flags = mmu2paging_flags(flags);
	mmu_entry_val_t old = atomic_load(entry);
	atomic_store(entry, (old & PAGING_ENTRY_ADDRESS) | paging_flags);
	return 0;
}

int mmu_entry_or_flags(mmu_space_t PML4, mmu_entry_t *entry, long flags) {
	(void)PML4;
	mmu_entry_val_t paging_flags = mmu2paging_flags(flags);
	atomic_fetch_or(entry, paging_flags);
	return 0;
}

long mmu_entry_get_and_clear_flags(mmu_space_t PML4, mmu_entry_t *entry, long flags) {
	(void)PML4;
	mmu_entry_val_t paging_flags = mmu2paging_flags(flags);
	mmu_entry_val_t old = atomic_fetch_and(entry, ~paging_flags);
	if (old & PAGING_FLAG_PRESENT) {
		return paging2mmu_flags(old & paging_flags);
	} else {
		return 0;
	}
}

mmu_entry_t *mmu_get_entry_and_acquire_lock(mmu_space_t PML4, uintptr_t vaddr) {
	rcu_acquire_read();
	mmu_entry_t *entry = mmu_get_entry(PML4, vaddr);
	if (!entry) return NULL;
	uintptr_t page = PAGE_ALIGN_DOWN(mmu_hhdm2phys(entry));
	pmm_retain(page);
	rcu_release_read();

	pmm_acquire_page_lock(page);
	pmm_release_page(page);
	return entry;
}

void mmu_entry_release_lock(mmu_entry_t *entry) {
	uintptr_t page = PAGE_ALIGN_DOWN(mmu_hhdm2phys(entry));
	pmm_release_page_lock(page);
}

mmu_entry_t *mmu_next_entry(mmu_space_t PML4, uintptr_t from, uintptr_t *vaddr, mmu_entry_t *old_entry) {
	uintptr_t old_page = PAGE_ALIGN_DOWN(mmu_hhdm2phys(old_entry));
	rcu_acquire_read();
	mmu_entry_t *entry = mmu_get_entry(PML4, from);
	if (!entry) {
		rcu_release_read();
		return NULL;
	}
	uintptr_t page = PAGE_ALIGN_DOWN(mmu_hhdm2phys(entry));
	if (page == old_page) {
		rcu_release_read();
	} else {
		pmm_release_page_lock(old_page);
		pmm_retain(page);
		rcu_release_read();
		pmm_acquire_page_lock(page);
	}
	return entry;
}

void mmu_map_kernel(uint64_t *PML4) {
	uint64_t kernel_start    = (uint64_t)*(&p_kernel_start);
	uint64_t kernel_end      = (uint64_t)*(&p_kernel_end);
	uint64_t kernel_text_end = (uint64_t)*(&p_kernel_text_end);

	uint64_t phys_page   = PAGE_ALIGN_DOWN(bootinfo_get_kernel_paddr());
	uint64_t virt_page   = PAGE_ALIGN_DOWN(kernel_start);
	uint64_t virt_end    = PAGE_ALIGN_DOWN(kernel_end);
	uint64_t text_end    = PAGE_ALIGN_DOWN(kernel_text_end);

	long flags = MMU_FLAG_READ | MMU_FLAG_PRESENT | MMU_FLAG_GLOBAL;
	mmu_map_pages(PML4, phys_page, virt_page, text_end - virt_page, flags | MMU_FLAG_EXEC);
	mmu_map_pages(PML4, phys_page + (text_end - virt_page), text_end, virt_end - text_end, flags | MMU_FLAG_WRITE);
}

void mmu_map_hhdm(uint64_t *PML4) {
	for (size_t index = 0; index < bootinfo_memmap_get_entries_count(); index++) {
		bootinfo_memmap_entry_t entry;
		bootinfo_memmap_get_entry(index, &entry);

		// map all the whole region
		long flags = MMU_FLAG_READ | MMU_FLAG_WRITE | MMU_FLAG_PRESENT | MMU_FLAG_GLOBAL;
		if (entry.type == LIMINE_MEMMAP_FRAMEBUFFER) {
			flags |= MMU_FLAG_WRITE_COMBINE;
		}
		uintptr_t phys_page = PAGE_ALIGN_DOWN(entry.start);
		uintptr_t phys_end  = PAGE_ALIGN_UP(entry.start + entry.size);
		uintptr_t virt_page = (uintptr_t)mmu_phys2virt(phys_page);
		mmu_map_pages(PML4, phys_page, virt_page, phys_end - phys_page, flags);
	}
}
