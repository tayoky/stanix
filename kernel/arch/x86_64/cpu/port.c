#include <kernel/port.h>
#include <stdint.h>

uint8_t in_byte(uint16_t port){
    uint8_t data;
    asm("inb %%dx, %%al": "=a" (data) : "d"(port) );
    return data;
}

void out_byte(uint16_t port,uint8_t data){
    asm("outb %%al, %%dx" : : "a" (data) , "d" (port) );
}

uint16_t in_word(uint16_t port){
    uint16_t data;
    asm("inw %%dx, %%ax": "=a" (data) : "d"(port) );
    return data;
}

void out_word(uint16_t port,uint16_t data){
    asm("outw %%ax, %%dx" : : "a" (data) , "d" (port) );
}

uint32_t in_long(uint16_t port){
	uint32_t data;
	asm volatile ("inl %%dx, %%eax" : "=a" (data) : "d" (port));
	return data;
}

void out_long(uint16_t port, uint32_t data){
	asm volatile ("outl %%eax, %%dx" : : "d" (port), "a" (data));
}