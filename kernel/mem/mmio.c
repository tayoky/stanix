#include <kernel/kernel.h>
#include <kernel/mmio.h>
#include <kernel/mmu.h>

volatile void *mmio_map(uintptr_t paddr, size_t size) {
	uintptr_t start = PAGE_ALIGN_DOWN(paddr);
	uintptr_t end   = PAGE_ALIGN_UP(paddr + size);
	void *vaddr     = mmu_phys2virt(paddr);
	
	uintptr_t vstart = (uintptr_t)mmu_phys2virt(start);
	if (mmu_map_pages(mmu_get_addr_space(), vstart, start, end - start, MMU_FLAG_GLOBAL | MMU_FLAG_READ | MMU_FLAG_WRITE | MMU_FLAG_UNCACHED | MMU_FLAG_PRESENT) < 0) {
		return NULL;
	}
	return vaddr;
}

void mmio_unmap(volatile void *vaddr, size_t size) {
	// TODO
	(void)vaddr;
	(void)size;
}
