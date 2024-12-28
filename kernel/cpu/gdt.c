#include "gdt.h"
#include "serial.h"
#include "kernel.h"
#include "asm.h"

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

#define GDT_SEGMENT_ACCESS_KERNEL GDT_SEGMENT_ACCESS_ACCESS \
| GDT_SEGMENT_ACCESS_PRESENT \
| GDT_SEGMENT_ACCESS_DPL_KERNEL \
| GDT_SEGMENT_ACCESS_RW \
| GDT_SEGMENT_ACCESS_S

#define GDT_SEGMENT_ACCESS_USER GDT_SEGMENT_ACCESS_ACCESS \
| GDT_SEGMENT_ACCESS_PRESENT \
| GDT_SEGMENT_ACCESS_DPL_USER \
| GDT_SEGMENT_ACCESS_RW \
| GDT_SEGMENT_ACCESS_S

void init_gdt(kernel_table *kernel){
    write_serial("init GDT ...");

    //first the null segment
    kernel->gdt[0] = create_gdt_segement(0,0,0,0);

    //kernel code and data
    kernel->gdt[1] = create_gdt_segement(0,0,GDT_SEGMENT_ACCESS_KERNEL | GDT_SEGMENT_ACCESS_EXECUTABLE,0x02);
    kernel->gdt[2] = create_gdt_segement(0,0,GDT_SEGMENT_ACCESS_KERNEL,0x00);

    //user code and data
    kernel->gdt[3] = create_gdt_segement(0,0,GDT_SEGMENT_ACCESS_USER | GDT_SEGMENT_ACCESS_EXECUTABLE,0X02);
    kernel->gdt[4] = create_gdt_segement(0,0,GDT_SEGMENT_ACCESS_USER,0x00);

    //create the GDTR anc load it so the GDT is actually used
    kernel->gdtr.size = sizeof(kernel->gdt);
    kernel->gdtr.offset = (uint64_t)&kernel->gdt[0];

    disable_interrupt();
    asm("lgdt (%0)" : : "r" (&kernel->gdtr));
    asm volatile("push $0x08; \
              lea .reload_seg(%%rip), %%rax; \
              push %%rax; \
              retfq; \
              .reload_seg: \
              mov $0x10, %%ax; \
              mov %%ax, %%ds; \
              mov %%ax, %%es; \
              mov %%ax, %%fs; \
              mov %%ax, %%gs; \
              mov %%ax, %%ss" : : : "eax", "rax");
    //later if i need tts
    //asm volatile("mov "something", %%ax\nltr %%ax" : : : "eax");
}