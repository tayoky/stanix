#include <kernel/kernel.h>
#include <kernel/arch.h>
#include <kernel/irq.h>

#define ICW1_ICW4	0x01		/* Indicates that ICW4 will be present */
#define ICW1_SINGLE	0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4	0x04		/* Call address interval 4 (8) */
#define ICW1_LEVEL	0x08		/* Level triggered (edge) mode */
#define ICW1_INIT	0x10		/* Initialization - required! */

#define ICW4_8086	0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO	0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	0x08		/* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C		/* Buffered mode/master */
#define ICW4_SFNM	0x10		/* Special fully nested (not) */

static void surpirous_handler(fault_frame_t *frame, void *arg) {
	(void)frame;
	(void)arg;
}

void init_pic(){
	// starts the initialization sequence (in cascade mode)
	out_byte(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
	out_byte(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

	//vector offset
	out_byte(PIC1_DATA, 32);
	out_byte(PIC2_DATA, 40);

	// ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	out_byte(PIC1_DATA, 4);

	// ICW3: tell Slave PIC its cascade identity (0000 0010)
	out_byte(PIC2_DATA, 2);
	
	//ICW4: have the PICs use 8086 mode (and not 8080 mode)
	out_byte(PIC1_DATA, ICW4_8086);
	out_byte(PIC2_DATA, ICW4_8086);

	//at the start unmask everything
	out_byte(PIC1_DATA, 0x00);
    out_byte(PIC2_DATA, 0x00);

	//tell the kernel we use pic
	kernel->pic_type = PIC_PIC;

	//map the surpirous isr
	irq_register_handler(7, surpirous_handler, NULL);
	irq_register_handler(15, surpirous_handler, NULL);
}

void pic_mask(uintmax_t irq_num) {
	uint8_t mask;
	uint16_t port;
	if(irq_num < 8){
		port = PIC1_DATA;
	} else {
		irq_num -= 8;
		port = PIC2_DATA;
	}
	mask = in_byte(port);
	mask |= 1 << irq_num;
	out_byte(port,mask);
}

void pic_unmask(uintmax_t irq_num){
	uint8_t mask;
	uint16_t port;
	if(irq_num < 8){
		port = PIC1_DATA;
	} else {
		irq_num -= 8;
		port = PIC2_DATA;
	}
	mask = in_byte(port);
	mask &= ~(1 << irq_num);
	out_byte(port,mask);
}

void pic_eoi(uintmax_t irq_num){
	if(irq_num >= 8){
		out_byte(PIC2_COMMAND,0x20);	
	}
	out_byte(PIC1_COMMAND,0x20);
}