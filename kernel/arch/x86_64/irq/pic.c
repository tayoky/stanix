#include <kernel/arch.h>
#include <kernel/asm.h>
#include <kernel/irq.h>
#include <kernel/kernel.h>

static irq_chip_t pic_chip;

static irq_t pic_irqs[16];

static void empty_handler(registers_t *frame, void *arg) {
	(void)frame;
	(void)arg;
}

void init_pic() {
	// setup the irq objects
	for (size_t i=0; i<arraylen(pic_irqs); i++) {
		pic_irqs[i].irqnum   = i;
		pic_irqs[i].hwirq    = i;
		irq_set_vector(&pic_irqs[i], i + 32);
		irq_add_to_chip(&pic_chip, &pic_irqs[i]);
	}

	// starts the initialization sequence (in cascade mode)
	out_byte(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
	io_wait();
	out_byte(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	io_wait();

	// vector offset
	out_byte(PIC1_DATA, 32);
	io_wait();
	out_byte(PIC2_DATA, 40);
	io_wait();

	// ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	out_byte(PIC1_DATA, 4);
	io_wait();

	// ICW3: tell Slave PIC its cascade identity (0000 0010)
	out_byte(PIC2_DATA, 2);
	io_wait();

	// ICW4: have the PICs use 8086 mode (and not 8080 mode)
	out_byte(PIC1_DATA, ICW4_8086);
	io_wait();
	out_byte(PIC2_DATA, ICW4_8086);
	io_wait();

	// at the start mask everything
	out_byte(PIC1_DATA, 0xff);
	out_byte(PIC2_DATA, 0xff);

	// tell the kernel we use pic
	main_irq_chip = &pic_chip;

	// unmask slave irq
	irq_register_handler(&pic_irqs[2], empty_handler, NULL);

	// map the surpirous isr
	irq_register_handler(&pic_irqs[7], empty_handler, NULL);
	irq_register_handler(&pic_irqs[15], empty_handler, NULL);
}

static void pic_mask(irq_chip_t *irq_chip, irq_t *irq) {
	(void)irq_chip;
	uint8_t mask;
	uint16_t port;
	irqnum_t irqnum = irq->irqnum;
	if (irqnum < 8) {
		port = PIC1_DATA;
	} else {
		irqnum -= 8;
		port = PIC2_DATA;
	}
	mask = in_byte(port);
	mask |= 1 << irqnum;
	out_byte(port, mask);
}

static void pic_unmask(irq_chip_t *irq_chip, irq_t *irq) {
	(void)irq_chip;
	uint8_t mask;
	uint16_t port;
	irqnum_t irqnum = irq->irqnum;
	if (irqnum < 8) {
		port = PIC1_DATA;
	} else {
		irqnum -= 8;
		port = PIC2_DATA;
	}
	mask = in_byte(port);
	mask &= ~(1 << irqnum);
	out_byte(port, mask);
}

static void pic_eoi(irq_chip_t *irq_chip, irq_t *irq) {
	(void)irq_chip;
	if (irq->irqnum >= 8) {
		out_byte(PIC2_COMMAND, 0x20);
	}
	out_byte(PIC1_COMMAND, 0x20);
}

static irq_t *pic_get_from_hwirq(irq_chip_t *irq_chip, hwirq_t hwirq) {
	(void)irq_chip;
	// map direcly hwirq to irqnum
	if (hwirq < 0 || hwirq >= 16) {
		return NULL;
	}
	return &pic_irqs[hwirq];
}

static irq_chip_t pic_chip = {
	.name             = "PIC",
	.type             = IRQ_CHIP_PIC,
	.mask             = pic_mask,
	.unmask           = pic_unmask,
	.eoi              = pic_eoi,
	.get_from_hwirq   = pic_get_from_hwirq,
};
