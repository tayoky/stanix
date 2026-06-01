#ifndef KERNEL_MMIO_H
#define KERNEL_MMIO_H

#include <stdint.h>

/**
 * @brief map physical memory as mmio
 * @param paddr the physical address of the memory region to map
 * @param size the size of the memory region to map
 * @return the allocated vitrual address
 */
volatile void *mmio_map(uintptr_t paddr, size_t size);

void mmio_unmap(volatile void *vaddr, size_t size);

static inline uint32_t mmio_read32(volatile void *mmio, size_t offset) {
	return *(volatile uint32_t*)((uintptr_t)mmio + offset);
}

static inline void mmio_write32(volatile void *mmio, size_t offset, uint32_t value) {
	*(volatile uint32_t*)((uintptr_t)mmio + offset) = value;
}

#endif
