#ifndef _KERNEL_IDT_H
#define _KERNEL_IDT_H
#include <stdint.h>

typedef struct {
    uint16_t offset1;
    uint16_t selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t offset2;
    uint32_t offset3;
    uint32_t reserved;
} __attribute__((packed)) idt_gate;

typedef struct {
    uint16_t size;
    uint64_t offset;
} __attribute__((packed)) IDTR;

void init_idt(void);
void exception_handler();

#endif