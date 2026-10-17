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

void mmu_invalidate(uintptr_t vaddr, size_t size) {
	while (size > 0) {
		asm volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
		vaddr += PAGE_SIZE;
		size  -= PAGE_SIZE;
	}
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

	mmu_space_t PML4 = mmu_phys2virt(page);

	// set every entry to 0
	memset(PML4, 0, PAGE_SIZE);

	// copy the shared PML4 entries
	memcpy(PML4 + (PAGING_ENTRIES_PER_LEVEL - SHARED_PML4_ENTRIES_COUNT), shared_PML4_entries, sizeof(uint64_t) * SHARED_PML4_ENTRIES_COUNT);

	kdebugf("create addrspace %p\n", PML4);
	return PML4;
}

void mmu_space_destroy(mmu_space_t PML4) {
	// recusively free everythings
	// EXCEPT THE SHARED PML4 entries
	kdebugf("free addrspace %p\n", PML4);
	for (uint16_t PML4i = 0; PML4i < (PAGING_ENTRIES_PER_LEVEL - SHARED_PML4_ENTRIES_COUNT); PML4i++) {
		if (!(PML4[PML4i] & 1)) continue;

		uint64_t *PDP = mmu_phys2virt(PML4[PML4i] & PAGING_ENTRY_ADDRESS);
		for (uint16_t PDPi = 0; PDPi < PAGING_ENTRIES_PER_LEVEL; PDPi++) {
			if (!(PDP[PDPi] & 1)) continue;
			uint64_t *PD = mmu_phys2virt(PDP[PDPi] & PAGING_ENTRY_ADDRESS);

			for (uint16_t PDi = 0; PDi < PAGING_ENTRIES_PER_LEVEL; PDi++) {
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

int mmu_map_pages(mmu_space_t PML4, uintptr_t vaddr, uintptr_t paddr, size_t size, long flags) {
	mmu_entry_t *entry = NULL;
	uintptr_t start = vaddr;
	while (size > 0) {
		entry = mmu_create_next_entry_and_acquire_lock(PML4, vaddr, entry, vaddr - PAGE_SIZE);
		if (entry) {
			mmu_unmap_pages(PML4, start, vaddr - start);
			return -ENOMEM;
		}
		mmu_entry_map(PML4, entry, paddr, flags);
		mmu_entry_release_lock(entry);
		paddr += PAGE_SIZE;
		vaddr += PAGE_SIZE;
		size  -= PAGE_SIZE;
	}
	if (entry) mmu_entry_release_lock(entry);
	return 0;
}

int mmu_allocate_and_map_pages(mmu_space_t PML4, uintptr_t vaddr, size_t size, long flags) {
	uintptr_t start = vaddr;
	while (size > 0) {
		uintptr_t page = pmm_allocate_page();
		if (page == PAGE_INVALID) {
			mmu_unmap_and_release_pages(PML4, start, vaddr - start);
			return -ENOMEM;
		}
		// FIXME : calling mmu_map_pages in a loop is bad
		int ret = mmu_map_pages(PML4, vaddr, page, PAGE_SIZE, flags);
		if (ret < 0) {
			pmm_release_page(page);
			mmu_unmap_and_release_pages(PML4, start, vaddr - start);
			return ret;
		}

		vaddr += PAGE_SIZE;
		size  -= PAGE_SIZE;
	}
	return 0;
}

void mmu_unmap_pages(mmu_space_t addrspace, uintptr_t vaddr, size_t size) {
	mmu_foreach_entry_range (current, entry, addrspace, vaddr, vaddr + size) {
		mmu_entry_unmap(addrspace, entry);
	}
}

void mmu_unmap_and_release_pages(mmu_space_t addrspace, uintptr_t vaddr, size_t size) {
	mmu_foreach_entry_range (current, entry, addrspace, vaddr, vaddr + size) {
		mmu_entry_val_t value = mmu_entry_unmap(addrspace, entry);
		uintptr_t page = mmu_entry_val_get_page(value);
		if (page != PAGE_INVALID) {
			pmm_release_page(page);
		}
	}
}

mmu_entry_t *mmu_get_entry(mmu_space_t PML4, uintptr_t vaddr) {
	rcu_acquire_read();
	mmu_entry_t *table = PML4;
	for (size_t level = PAGING_LEVEL_MAX; level > 0; level--) {
		size_t index = PAGING_LEVEL_INDEX(level, vaddr);
		mmu_entry_val_t current = atomic_load(&table[index]);
		if (!(current & PAGING_FLAG_PRESENT)) {
			rcu_release_read();
			return NULL;
		}

		table = mmu_phys2virt(mmu_entry_val_get_page(current));
	}

	size_t index = PAGING_LEVEL_INDEX(0, vaddr);
	rcu_release_read();
	return &table[index];
}

mmu_entry_t *mmu_get_entry_and_acquire_lock(mmu_space_t PML4, uintptr_t vaddr) {
	mmu_entry_t *table = PML4;
	pmm_acquire_page_lock(mmu_hhdm2phys(table));
	for (size_t level = PAGING_LEVEL_MAX; level > 0; level--) {
		size_t index = PAGING_LEVEL_INDEX(level, vaddr);
		mmu_entry_val_t current = atomic_load(&table[index]);
		if (!(current & PAGING_FLAG_PRESENT)) {
			pmm_release_page_lock(mmu_hhdm2phys(table));
			return NULL;
		}

		mmu_entry_t *next_table = mmu_phys2virt(mmu_entry_val_get_page(current));
		pmm_acquire_page_lock(mmu_hhdm2phys(next_table));
		pmm_release_page_lock(mmu_hhdm2phys(table));
		table = next_table;
	}

	size_t index = PAGING_LEVEL_INDEX(0, vaddr);
	return &table[index];
}

mmu_entry_t *mmu_create_entry_and_acquire_lock(mmu_space_t PML4, uintptr_t vaddr) {
	mmu_entry_t *table = PML4;
	pmm_acquire_page_lock(mmu_hhdm2phys(table));
	for (size_t level = PAGING_LEVEL_MAX; level > 0; level--) {
		size_t index = PAGING_LEVEL_INDEX(level, vaddr);
		mmu_entry_val_t current = atomic_load(&table[index]);
		if (!(current & PAGING_FLAG_PRESENT)) {
			uintptr_t page = pmm_allocate_page();
			if (page == PAGE_INVALID) {
				pmm_release_page_lock(mmu_hhdm2phys(table));
				return NULL;
			}
			current = page | PAGING_FLAG_RW_CPL3;
			atomic_store(&table[index], current);
		}

		mmu_entry_t *next_table = mmu_phys2virt(mmu_entry_val_get_page(current));
		pmm_acquire_page_lock(mmu_hhdm2phys(next_table));
		pmm_release_page_lock(mmu_hhdm2phys(table));
		table = next_table;
	}

	size_t index = PAGING_LEVEL_INDEX(0, vaddr);
	return &table[index];
}

static mmu_entry_t *mmu_get_next_entry_helper(mmu_entry_t *table, size_t level, uintptr_t from, uintptr_t *vaddr) {
	size_t start = PAGING_LEVEL_INDEX(level, from);
	for (size_t i = start; i < PAGING_ENTRIES_PER_LEVEL; i++) {
		mmu_entry_val_t current = atomic_load(&table[i]);
		if (!(current & PAGING_FLAG_PRESENT)) continue;

		mmu_entry_t *entry;
		if (level > 0) {
			mmu_entry_t *sub_table = mmu_phys2virt(current & PAGING_ENTRY_ADDRESS);
			entry = mmu_get_next_entry_helper(sub_table, level - 1, i == start ? from : 0, vaddr);
		} else {
			entry = &table[i];
		}
		if (!entry) continue;
		vaddr += i << PAGING_LEVEL_SHIFT(level);
		return entry;
	}
	return NULL;
}

mmu_entry_t *mmu_get_next_entry(mmu_space_t PML4, uintptr_t from, uintptr_t *vaddr) {
	rcu_acquire_read();
	mmu_entry_t *entry = mmu_get_next_entry_helper(PML4, PAGING_LEVEL_MAX, from, vaddr);
	rcu_release_read();
	return entry;
}

mmu_entry_t *mmu_get_next_entry_and_acquire_lock(mmu_space_t PML4, uintptr_t from, uintptr_t *vaddr, mmu_entry_t *old_entry, uintptr_t old_vaddr) {
	if (old_entry && (from & (PAGING_ENTRIES_PER_LEVEL * PAGE_SIZE)) == (old_vaddr & (PAGING_ENTRIES_PER_LEVEL * PAGE_SIZE))) {
		// we can attempt a fast search
		mmu_entry_t *table = (mmu_entry_t *)PAGE_ALIGN_DOWN((uintptr_t)old_entry);
		size_t start;
		if (from > old_vaddr) {
			start = old_entry - table + (from - old_vaddr);
		} else {
			start = old_entry - table - (old_vaddr - from);
		}
		for (size_t i = start; i < PAGING_ENTRIES_PER_LEVEL; i++) {
			if (atomic_load(&table[i]) & PAGING_FLAG_PRESENT) {
				return &table[i];
			}
		}
	}

	if (old_entry) mmu_entry_release_lock(old_entry);
	// TODO : do a classic slow search
	return NULL;
}

mmu_entry_t *mmu_create_next_entry_and_acquire_lock(mmu_space_t PML4, uintptr_t vaddr, mmu_entry_t *old_entry, uintptr_t old_vaddr) {
	if (!old_entry) return mmu_create_entry_and_acquire_lock(PML4, vaddr);

	if ((vaddr & (PAGING_ENTRIES_PER_LEVEL * PAGE_SIZE)) == (old_vaddr & (PAGING_ENTRIES_PER_LEVEL * PAGE_SIZE))) {
		// the two entries are on the same PT
		if (vaddr > old_vaddr) {
			size_t diff = vaddr - old_vaddr;
			return old_entry + diff / PAGE_SIZE;
		} else {
			size_t diff = old_vaddr - vaddr;
			return old_entry - diff / PAGE_SIZE;
		}
	}

	mmu_entry_release_lock(old_entry);
	return mmu_create_entry_and_acquire_lock(PML4, vaddr);
}

void mmu_entry_release_lock(mmu_entry_t *entry) {
	uintptr_t page = PAGE_ALIGN_DOWN(mmu_hhdm2phys(entry));
	pmm_release_page_lock(page);
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

uintptr_t mmu_virt2phys(void *address) {
	// just wrap arround space_virt2phys
	return mmu_space_virt2phys(mmu_get_addr_space(), address);
}

void mmu_entry_map(mmu_space_t PML4, mmu_entry_t *entry, uintptr_t paddr, long flags) {
	(void)PML4;
	mmu_entry_val_t paging_flags = mmu2paging_flags(flags);
	atomic_store(entry, (paddr & PAGING_ENTRY_ADDRESS) | paging_flags);
}

void mmu_entry_map_and_or_flags(mmu_space_t addrspace, mmu_entry_t *entry, uintptr_t paddr, long flags) {
	(void)addrspace;
	mmu_entry_val_t paging_flags = mmu2paging_flags(flags);
	mmu_entry_val_t expected = atomic_load(entry);
	mmu_entry_val_t new_val = (paddr & PAGING_ENTRY_ADDRESS) | (expected & PAGING_ENTRY_FLAGS) | paging_flags;
	while (!atomic_compare_exchange_weak(entry, &expected, new_val)) {
		new_val = (paddr & PAGING_ENTRY_ADDRESS) | (expected & PAGING_ENTRY_FLAGS) | paging_flags;
	}
}

mmu_entry_val_t mmu_entry_unmap(mmu_space_t addrspace, mmu_entry_t *entry) {
	(void)addrspace;
	return atomic_exchange(entry, 0);
}

long mmu_entry_get_flags(mmu_space_t PML4, mmu_entry_t *entry) {
	(void)PML4;
	mmu_entry_val_t value = mmu_entry_fetch(entry);
	return mmu_entry_val_get_flags(value);
}

uintptr_t mmu_entry_get_page(mmu_space_t PML4, mmu_entry_t *entry) {
	(void)PML4;
	mmu_entry_val_t value = mmu_entry_fetch(entry);
	return mmu_entry_val_get_page(value);
}

mmu_entry_val_t mmu_entry_set_flags(mmu_space_t PML4, mmu_entry_t *entry, long flags) {
	(void)PML4;
	mmu_entry_val_t paging_flags = mmu2paging_flags(flags);
	mmu_entry_val_t expected = atomic_load(entry);
	mmu_entry_val_t new_val = (expected & PAGING_ENTRY_ADDRESS) | paging_flags;
	while (!atomic_compare_exchange_weak(entry, &expected, new_val)) {
		new_val = (expected & PAGING_ENTRY_ADDRESS) | paging_flags;
	}
	return expected;
}

mmu_entry_val_t mmu_entry_or_flags(mmu_space_t PML4, mmu_entry_t *entry, long flags) {
	(void)PML4;
	mmu_entry_val_t paging_flags = mmu2paging_flags(flags);
	return atomic_fetch_or(entry, paging_flags);
}

mmu_entry_val_t mmu_entry_clear_flags(mmu_space_t PML4, mmu_entry_t *entry, long flags) {
	(void)PML4;
	mmu_entry_val_t paging_flags = mmu2paging_flags(flags);
	return atomic_fetch_and(entry, ~paging_flags);
}

uintptr_t mmu_entry_val_get_page(mmu_entry_val_t entry_val) {
	if (entry_val & PAGING_FLAG_PRESENT) {
		return entry_val & PAGING_ENTRY_ADDRESS;
	} else {
		return PAGE_INVALID;
	}
}

long mmu_entry_val_get_flags(mmu_entry_val_t entry_val) {
	if (entry_val & PAGING_FLAG_PRESENT) {
		long flags = paging2mmu_flags(entry_val);
		return flags;
	} else {
		return 0;
	}
}

void mmu_map_kernel(mmu_space_t PML4) {
	uint64_t kernel_start    = (uint64_t)*(&p_kernel_start);
	uint64_t kernel_end      = (uint64_t)*(&p_kernel_end);
	uint64_t kernel_text_end = (uint64_t)*(&p_kernel_text_end);

	uint64_t phys_page   = PAGE_ALIGN_DOWN(bootinfo_get_kernel_paddr());
	uint64_t virt_page   = PAGE_ALIGN_DOWN(kernel_start);
	uint64_t virt_end    = PAGE_ALIGN_DOWN(kernel_end);
	uint64_t text_end    = PAGE_ALIGN_DOWN(kernel_text_end);

	long flags = MMU_FLAG_READ | MMU_FLAG_PRESENT | MMU_FLAG_GLOBAL;
	mmu_map_pages(PML4, virt_page, phys_page, text_end - virt_page, flags | MMU_FLAG_EXEC);
	mmu_map_pages(PML4, text_end, phys_page + (text_end - virt_page), virt_end - text_end, flags | MMU_FLAG_WRITE);
}

void mmu_map_hhdm(mmu_space_t PML4) {
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
		mmu_map_pages(PML4, virt_page, phys_page, phys_end - phys_page, flags);
	}
}
