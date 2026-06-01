#include <kernel/arch.h>
#include <kernel/irq.h>
#include <kernel/kernel.h>

static irq_chip_t pic_chip;

static void surpirous_handler(registers_t *frame, void *arg) {
	(void)frame;
	(void)arg;
}

void init_pic() {
	// starts the initialization sequence (in cascade mode)
	out_byte(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
	out_byte(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

	// vector offset
	out_byte(PIC1_DATA, 32);
	out_byte(PIC2_DATA, 40);

	// ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	out_byte(PIC1_DATA, 4);

	// ICW3: tell Slave PIC its cascade identity (0000 0010)
	out_byte(PIC2_DATA, 2);

	// ICW4: have the PICs use 8086 mode (and not 8080 mode)
	out_byte(PIC1_DATA, ICW4_8086);
	out_byte(PIC2_DATA, ICW4_8086);

	// at the start mask everything
	out_byte(PIC1_DATA, 0xff);
	out_byte(PIC2_DATA, 0xff);

	// tell the kernel we use pic
	irq_chip = &pic_chip;

	// map the surpirous isr
	irq_register_handler(7, surpirous_handler, NULL);
	irq_register_handler(15, surpirous_handler, NULL);
}

static void pic_mask(irqnum_t irq_num) {
	uint8_t mask;
	uint16_t port;
	if (irq_num < 8) {
		port = PIC1_DATA;
	} else {
		irq_num -= 8;
		port = PIC2_DATA;
	}
	mask = in_byte(port);
	mask |= 1 << irq_num;
	out_byte(port, mask);
}

static void pic_unmask(irqnum_t irq_num) {
	uint8_t mask;
	uint16_t port;
	if (irq_num < 8) {
		port = PIC1_DATA;
	} else {
		irq_num -= 8;
		port = PIC2_DATA;
	}
	mask = in_byte(port);
	mask &= ~(1 << irq_num);
	out_byte(port, mask);
}

static void pic_eoi(irqnum_t irq_num) {
	if (irq_num > 16) return;
	if (irq_num >= 8) {
		out_byte(PIC2_COMMAND, 0x20);
	}
	out_byte(PIC1_COMMAND, 0x20);
}

static void pic_register_handler(irqnum_t irq_num, void *handler, void *data) {
	idt_register_handler(irq_num + 32, handler, data);
}

static irqnum_t pic_hirq2irq(int hirq) {
	return hirq;
}

static irq_chip_t pic_chip = {
	.name             = "PIC",
	.type             = IRQ_CHIP_PIC,
	.mask             = pic_mask,
	.unmask           = pic_unmask,
	.eoi              = pic_eoi,
	.register_handler = pic_register_handler,
	.hirq2irq         = pic_hirq2irq,
};
