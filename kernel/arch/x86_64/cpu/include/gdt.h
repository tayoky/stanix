#ifndef GDT_H
#define GDT_H
#include <stdint.h>

#define GDT_SEGMENT_ACCESS_ACCESS 0x01
#define GDT_SEGMENT_ACCESS_RW 0x02
#define GDT_SEGMENT_ACCESS_DC 0X04
#define GDT_SEGMENT_ACCESS_EXECUTABLE 0x08
#define GDT_SEGMENT_ACCESS_S 0x10
#define GDT_SEGMENT_ACCESS_DPL_KERNEL 0X00
#define GDT_SEGMENT_ACCESS_DPL_USER 0X60
#define GDT_SEGMENT_ACCESS_PRESENT 0X80

typedef struct {
	uint16_t limit;
	uint16_t base1;
	uint8_t base2;
	uint8_t access;
	uint8_t flags;
	uint8_t base3;
} __attribute__((packed)) gdt_segment;

typedef struct {
	uint16_t size;
	uint64_t offset;
} __attribute__((packed)) GDTR;

void init_gdt(void);

gdt_segment create_gdt_segement(uint64_t base,uint64_t limit,uint8_t access,uint8_t falgs);
#endif