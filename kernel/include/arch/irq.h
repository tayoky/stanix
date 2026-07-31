#ifndef KERNEL_IRQ_H
#define KERNEL_IRQ_H

#include <kernel/interrupt.h>
#include <kernel/uch.h>
#include <kernel/list.h>
#include <stdint.h>

typedef int hwirq_t;
typedef int irqnum_t;

#define IRQ_NO_IRQNUM -1
#define IRQ_NO_HWIRQ  -1
#define IRQ_VECTOR_ALLOCATE -1

typedef struct irq {
	list_t handlers;
	struct irq_chip *irq_chip;
	irqnum_t irqnum; // iternal irq num (gsi for APIC, ...)
	hwirq_t hwirq;
	intrnum_t vector;
} irq_t;

typedef struct irq_chip irq_chip_t;
struct irq_chip {
	// mandatory operations
	void (*mask)(irq_chip_t *irq_chip, irq_t *irq);
	void (*unmask)(irq_chip_t *irq_chip, irq_t *irq);
	void (*eoi)(irq_chip_t *irq_chip, irq_t *irq);

	// optionals
	irq_t *(*get_from_irqnum)(irq_chip_t *irq_chip, irqnum_t irqnum);
	irq_t *(*get_from_hwirq)(irq_chip_t *irq_chip, hwirq_t hwirq);
	irq_t *(*allocate)(irq_chip_t *irq_chip);
	void (*free)(irq_chip_t *irq_chip, irq_t *irq);

	list_t irqs;
	const char *name;
	int type;
};

extern irq_chip_t *main_irq_chip;

void init_irq(void);
void irq_mask(irq_t *irq);
void irq_unmask(irq_t *irq);
void irq_eoi(irq_t *irq);
irq_t *irq_get_from_irqnum(irq_chip_t *irq_chip, irqnum_t irqnum);
irq_t *irq_get_from_hwirq(irq_chip_t *irq_chip, hwirq_t hwirq);
irq_t *irq_allocate(irq_chip_t *irq_chip);
void irq_free(irq_t *irq);

// these functions should only be called from irq chip
void irq_set_vector(irq_t *irq, intrnum_t vector);
void irq_add_to_chip(irq_chip_t *irq_chip, irq_t *irq);
irq_t *irq_allocate_object(irqnum_t irqnum, hwirq_t hwirq);

irq_t *irq_from_vector(intrnum_t vector);
void irq_dispatch_vector(intrnum_t vector);

// OLD
irqnum_t irq_hirq2irq(int hirq);
void irq_old_register_handler(irqnum_t irq_num, interrupt_handler_t handler, void *data);

void *irq_register_handler(irq_t *irq, interrupt_handler_t handler, void *data);
void irq_unregister_handler(irq_t *irq, void *handle);
void irq_handle(irq_t *irq, registers_t *registers);

#endif
