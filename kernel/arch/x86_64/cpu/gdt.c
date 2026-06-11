#include <kernel/arch.h>
#include <kernel/print.h>
#include <kernel/kernel.h>


static gdt_segment gdt[7];
static GDTR gdtr;

gdt_segment create_gdt_segment(uint64_t base,uint64_t limit,uint8_t access,uint8_t flags){
	gdt_segment result;
	result.base1 = base & 0xFFFF;
	result.base2 = (base >> 16) & 0xFF;
	result.base3 = (base >> 24) & 0xFF;
	result.limit = limit & 0xFFFF;
	result.flags = (limit >> 16) & 0x0F;
	result.access = access;
	result.flags = (flags << 4) & 0xF0;
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

void init_gdt(void){
	kstatusf("init GDT... ");

	//first the null segment
	gdt[0] = create_gdt_segment(0,0,0,0);

	//kernel code and data
	gdt[1] = create_gdt_segment(0,0,GDT_SEGMENT_ACCESS_KERNEL | GDT_SEGMENT_ACCESS_EXECUTABLE,0x02);
	gdt[2] = create_gdt_segment(0,0,GDT_SEGMENT_ACCESS_KERNEL,0x00);

	//user code and data
	gdt[3] = create_gdt_segment(0,0,0xFA,0x02);
	gdt[4] = create_gdt_segment(0,0,0xF2,0x00);

	//tss take two entries
	gdt[5] = create_gdt_segment((uint64_t)&tss & 0xFFFFFFFF,sizeof(TSS) - 1,0x89,0);
	uint64_t tss_addressH = ((uint64_t)&tss >> 32) & 0xFFFFFFFF;
	gdt[6] = *(gdt_segment *)&tss_addressH;

	//create the GDTR and load it so the GDT is actually used
	gdtr.size = sizeof(gdt) - 1;
	gdtr.offset = (uint64_t)&gdt;

	asm("lgdt %0" : : "m" (gdtr));

	asm volatile("push $0x08; \
				lea reload_seg(%%rip), %%rax; \
				push %%rax; \
				retfq; \
				reload_seg: \
				mov $0x10, %%ax; \
				mov %%ax, %%ds; \
				mov %%ax, %%es; \
				mov %%ax, %%fs; \
				mov %%ax, %%gs; \
				mov %%ax, %%ss" : : : "eax", "rax");
	kok();
}