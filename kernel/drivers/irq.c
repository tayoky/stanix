#include <kernel/irq.h>
#include <kernel/print.h>
#include <kernel/slab.h>
#include <kernel/panic.h>
#include <kernel/string.h>

// irq manager

irq_chip_t *main_irq_chip;

typedef struct irq_handler {
	list_node_t node;
	interrupt_handler_t handler;
	void *data;
} irq_handler_t;

static slab_cache_t irq_handlers_slab;
static slab_cache_t irqs_slab;

// TODO : make this table dynamic
static irq_t *vector2irq[256];

void init_irq(void) {
	slab_init(&irq_handlers_slab, sizeof(irq_handler_t), "irq-handlers");
	slab_init(&irqs_slab, sizeof(irq_t), "irqs");
	init_arch_irq();
}

#define IRQ_CHIP_OP(irq_chip, op, ...) \
	if (irq_chip->op) { \
		return irq_chip->op(irq_chip, __VA_ARGS__); \
	} else { \
		kwarningf("unimplemented operation '%s' for irq chip '%s'\n", #op, irq_chip->name); \
	}

#define IRQ_CHIP_OPTIONAL_OP(irq_chip, op, ...) \
	if (irq_chip->op) { \
		return irq_chip->op(irq_chip, __VA_ARGS__); \
	}

void irq_mask(irq_t *irq) {
	IRQ_CHIP_OP(irq->irq_chip, mask, irq);
}

void irq_unmask(irq_t *irq) {
	IRQ_CHIP_OP(irq->irq_chip, unmask, irq);
}

void irq_eoi(irq_t *irq) {
	IRQ_CHIP_OP(irq->irq_chip, eoi, irq);
}

irq_t *irq_get_from_irqnum(irq_chip_t *irq_chip, irqnum_t irqnum) {
	IRQ_CHIP_OPTIONAL_OP(irq_chip, get_from_irqnum, irqnum);
	foreach (node, &irq_chip->irqs) {
		irq_t *irq = container_of(node, irq_t, node);
		if (irq->irqnum == irqnum) {
			return irq;
		}
	}
	return NULL;
}

irq_t *irq_get_from_hwirq(irq_chip_t *irq_chip, hwirq_t hwirq) {
	IRQ_CHIP_OPTIONAL_OP(irq_chip, get_from_hwirq, hwirq);
	foreach (node, &irq_chip->irqs) {
		irq_t *irq = container_of(node, irq_t, node);
		if (irq->hwirq == hwirq) {
			return irq;
		}
	}
	return NULL;
}

uintptr_t irq_msi_get_address(irq_t *irq) {
	IRQ_CHIP_OP(irq->irq_chip, msi_get_address, irq);
	return 0;
}

uint32_t irq_msi_get_data(irq_t *irq) {
	IRQ_CHIP_OP(irq->irq_chip, msi_get_data, irq);
	return 0;
}

void init_irq_chip(irq_chip_t *irq_chip) {
	rman_init(&irq_chip->rman, RESOURCE_IRQ, irq_chip->name);
}

void irq_set_vector(irq_t *irq, intrnum_t vector) {
	irq->vector = vector;
	kassert (vector >= 0 && vector < (intrnum_t)arraylen(vector2irq));
	vector2irq[vector] = irq;
}

void irq_add_to_chip(irq_chip_t *irq_chip, irq_t *irq) {
	irq->irq_chip = irq_chip;
	list_append(&irq_chip->irqs, &irq->node);
	rman_add_region(&irq_chip->rman, irq->irqnum, 1);
}

irq_t *irq_allocate_object(irqnum_t irqnum, hwirq_t hwirq) {
	irq_t *irq = slab_alloc(&irqs_slab);
	if (!irq) return NULL;
	memset(irq, 0, sizeof(irq_t));
	irq->irqnum = irqnum;
	irq->hwirq  = hwirq;
	return irq;
}

irq_t *irq_from_vector(intrnum_t vector) {
	if (vector < 0 || vector >= (intrnum_t)arraylen(vector2irq)) {
		return NULL;
	}
	return vector2irq[vector];
}

void irq_dispatch_vector(intrnum_t vector, registers_t *registers) {
	irq_t *irq = irq_from_vector(vector);
	if (irq) {
		irq_eoi(irq);
		irq_handle(irq, registers);
	}
}

void *irq_register_handler(irq_t *irq, interrupt_handler_t handler, void *data) {
	if (!irq || !handler) return NULL;
	irq_handler_t *irq_handler = slab_alloc(&irq_handlers_slab);
	if (!irq_handler) return NULL;
	irq_handler->handler = handler;
	irq_handler->data    = data;
	list_append(&irq->handlers, &irq_handler->node);

	// now unmask
	irq_unmask(irq);
	return irq_handler;
}

void irq_unregister_handler(irq_t *irq, void *handle) {
	if (!irq || !handle) return;
	irq_handler_t *irq_handler = handle;
	list_remove(&irq->handlers, &irq_handler->node);
	slab_free(irq_handler);

	if (list_is_empty(&irq->handlers)) {
		// no more handlers on this irq
		irq_mask(irq);
	}
}

void irq_handle(irq_t *irq, registers_t *registers) {
	if (!irq) return;
	foreach (node, &irq->handlers) {
		irq_handler_t *irq_handler = container_of(node, irq_handler_t, node);
		if (irq_handler->handler) {
			irq_handler->handler(registers, irq_handler->data);
		}
	}
}
