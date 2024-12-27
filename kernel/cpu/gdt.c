#include "gdt.h"
#include "serial.h"
#include "kernel.h"

gdt_segment create_gdt_segement(uint64_t base,uint64_t limit,uint8_t access,uint8_t falgs){
    gdt_segment result;
    result.base1 = base & 0xFFFF;
    result.base2 = (base >> 16) & 0xFF;
    result.base3 = (base >> 24) & 0xFF;
    result.limit = limit & 0xFFFF;
    result.flags = (limit >> 16) & 0x0F;
    result.access = access;
    result.flags = (access << 4) & 0xF0;
    return result;
}

void init_gdt(kernel_table *kernel){
    write_serial("init serial ...");

    //first the null segment
    kernel->gdt[0] = create_gdt_segement(0,0,0,0);
}