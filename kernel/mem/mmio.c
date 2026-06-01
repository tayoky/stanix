#include <kernel/kernel.h>
#include <kernel/mmio.h>
#include <kernel/mmu.h>

volatile void *mmio_map(uintptr_t paddr, size_t size) {
	// TODO : do not rely on hhdm
	uintptr_t start = PAGE_ALIGN_DOWN(paddr);
	uintptr_t end   = PAGE_ALIGN_UP(paddr + size);
	void *vaddr = (void *)(paddr + kernel->hhdm);
	if (mmu_virt2phys(vaddr) != PAGE_INVALID) {
		// already mapped
		return vaddr;
	}
	size_t pages_count = (end - start) / PAGE_SIZE;
	uintptr_t vcurrent = start + kernel->hhdm;
	uintptr_t pcurrent = start;
	for (size_t i = 0; i < pages_count; i++) {
		// TODO : map as uncacheable
		mmu_map_page(mmu_get_addr_space(), pcurrent, vcurrent, MMU_FLAG_GLOBAL | MMU_FLAG_READ | MMU_FLAG_WRITE | MMU_FLAG_PRESENT);
		pcurrent += PAGE_SIZE;
		vcurrent += PAGE_SIZE;
	}
	return vaddr;
}

void mmio_unmap(volatile void *vaddr, size_t size) {
	// TODO
	(void)vaddr;
	(void)size;
}
