#ifndef KERNEL_H
#define KERNEL_H
#include "gdt.h"

typedef struct {
    gdt_segment gdt[5];
}kernel_table;

#endif