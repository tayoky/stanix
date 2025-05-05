#ifndef PIC_H
#define PIC_H

#include <stdint.h>

#define PIC1		0x20		/* IO base address for master PIC */
#define PIC2		0xA0		/* IO base address for slave PIC */
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)

void init_pic(void);

void pic_mask(uintmax_t irq_num);
void pic_unmask(uintmax_t irq_num);
void pic_eoi(uintmax_t irq_num);

#endif