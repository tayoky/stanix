#ifndef KERNEL_IRQ_H
#define KERNEL_IRQ_H

#include <kernel/interrupt.h>
#include <kernel/resource.h>
#include <kernel/arch.h>
#include <kernel/list.h>
#include <stdint.h>

typedef int hwirq_t;
typedef int irqnum_t;

#define IRQ_NO_HWIRQ  -1
#define IRQ_MSI_START 0x1000
#define IRQ_MSI_END   0x2000

typedef struct irq {
	list_node_t node;
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
	uintptr_t (*msi_get_address)(irq_chip_t *irq_chip, irq_t *irq);
	uint32_t (*msi_get_data)(irq_chip_t *irq_chip, irq_t *irq);

	list_t irqs;
	rman_t rman;
	const char *name;
	int type;
};

extern irq_chip_t *main_irq_chip;

void init_irq(void);

// irq functions
irq_t *irq_get_from_irqnum(irq_chip_t *irq_chip, irqnum_t irqnum);
irq_t *irq_get_from_hwirq(irq_chip_t *irq_chip, hwirq_t hwirq);
uintptr_t irq_msi_get_address(irq_t *irq);
uint32_t irq_msi_get_data(irq_t *irq);
void *irq_register_handler(irq_t *irq, interrupt_handler_t handler, void *data);
void irq_unregister_handler(irq_t *irq, void *handle);

// irq chip functions
void init_irq_chip(irq_chip_t *irq_chip);
void irq_set_vector(irq_t *irq, intrnum_t vector);
void irq_add_to_chip(irq_chip_t *irq_chip, irq_t *irq);
irq_t *irq_allocate_object(irqnum_t irqnum, hwirq_t hwirq);

// low level functions
void irq_mask(irq_t *irq);
void irq_unmask(irq_t *irq);
void irq_eoi(irq_t *irq);
irq_t *irq_from_vector(intrnum_t vector);
void irq_dispatch_vector(intrnum_t vector, registers_t *registers);
void irq_handle(irq_t *irq, registers_t *registers);

#endif
