#ifndef GDT_H
#define GDT_H
#include <stdint.h>

typedef struct {
    uint16_t limit;
    uint16_t base1;
    uint8_t base2;
    uint8_t access;
    uint8_t flags;
    uint8_t base3;
} __attribute__((packed)) gdt_segment;

#include "kernel.h"

void init_gdt(kernel_table *kernel);

gdt_segment create_gdt_segement(uint64_t base,uint64_t limit,uint8_t access,uint8_t falgs);
#endif