#ifndef KERNEL_PORT_H
#define KERNEL_PORT_H

#include <stdint.h>

#if defined(__KERNEL__) || defined(MODULE)

/**
 * @brief read an given port
 * @param port the port to read
 * @return the data on the given port
 */
static inline uint8_t in_byte(uint16_t port) {
	uint8_t data;
	asm volatile("inb %%dx, %%al": "=a" (data) : "d"(port));
	return data;
}

/**
 * @brief out on a given port
 * @param port the port to out
 * @param data the data to out
 */
static inline void out_byte(uint16_t port, uint8_t data) {
	asm volatile("outb %%al, %%dx" : : "a" (data), "d" (port));
}

static inline uint16_t in_word(uint16_t port) {
	uint16_t data;
	asm volatile("inw %%dx, %%ax": "=a" (data) : "d"(port));
	return data;
}

static inline void out_word(uint16_t port, uint16_t data) {
	asm volatile("outw %%ax, %%dx" : : "a" (data), "d" (port));
}

static inline uint32_t in_long(uint16_t port) {
	uint32_t data;
	asm volatile("inl %%dx, %%eax" : "=a" (data) : "d" (port));
	return data;
}

static inline void out_long(uint16_t port, uint32_t data) {
	asm volatile("outl %%eax, %%dx" : : "d" (port), "a" (data));
}

#endif

#endif
