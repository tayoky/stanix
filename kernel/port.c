#include "port.h"
#include <stdint.h>

uint8_t in_byte(uint16_t port){
    uint8_t data;
    asm("inb %%dx, %%al": "=a" (data) : "d"(port) );
    return data;
}

void out_byte(uint16_t port,uint8_t data){
    asm("outb %%al, %%dx" : : "a" (data) , "d" (port) );
}