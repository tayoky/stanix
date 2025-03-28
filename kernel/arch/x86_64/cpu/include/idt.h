#ifndef IDT_H
#define IDT_H
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

void set_idt_gate(idt_gate *idt,uint8_t index,void *offset,uint8_t flags);
void init_idt(void);
void exception_handler();

#endif