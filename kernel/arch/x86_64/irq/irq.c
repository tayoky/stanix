#include <kernel/arch.h>
#include <kernel/irq.h>
#include <kernel/kernel.h>
#include <kernel/print.h>

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

static interrupt_handler_t handlers[256 - 32];
static void *handlers_data[256 - 32];
irq_chip_t *irq_chip;

void init_irq(void) {
	kstatusf("init irq chip... ");

	if (have_apic()) {
		init_apic();
	} else {
		init_pic();
	}

	kok();

	kinfof("using irq chip '%s'\n", irq_chip->name);
	enable_interrupt();
}

irqnum_t irq_allocate(interrupt_handler_t handler, void *data) {
	int i = 64;
	while (i < 256 - 32) {
		if (!handlers[i]) {
			handlers[i] = handler;
			irq_register_handler(i, handler, data);
			return i;
		}
		i++;
	}
	return -1;
}

void irq_mask(irqnum_t irq_num) {
	if (irq_chip->mask) {
		irq_chip->mask(irq_num);
	} else {
		kwarningf("unimplemented mask operation for irq chip '%s'\n", irq_chip->name);
	}
}

void irq_unmask(irqnum_t irq_num) {
	if (irq_chip->unmask) {
		irq_chip->unmask(irq_num);
	} else {
		kwarningf("unimplemented unmask operation for irq chip '%s'\n", irq_chip->name);
	}
}

void irq_eoi(irqnum_t irq_num) {
	if (irq_chip->eoi) {
		irq_chip->eoi(irq_num);
	} else {
		kwarningf("unimplemented eoi operation for irq chip '%s'\n", irq_chip->name);
	}
}

void irq_register_handler(irqnum_t irq_num, interrupt_handler_t handler, void *data) {
	// save the handler
	handlers[irq_num]      = handler;
	handlers_data[irq_num] = data;

	// and then mask/unmask it
	if (!irq_chip) return;
	if (handler) {
		irq_unmask(irq_num);
	} else {
		irq_mask(irq_num);
	}
}

void irq_handler(registers_t *frame) {
	interrupt_handler_t handler = handlers[frame->err_type - 32];
	void *data                  = handlers_data[frame->err_type - 32];
	frame->err_code             = frame->err_type - 32;
	if (frame->err_type < 48) {
		irq_eoi(frame->err_code);
	}
	if (handler) {
		handler(frame, data);
	}
}
