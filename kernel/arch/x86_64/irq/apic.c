#include <kernel/acpi.h>
#include <kernel/arch.h>
#include <kernel/irq.h>
#include <kernel/kernel.h>
#include <kernel/kheap.h>
#include <kernel/mmio.h>
#include <kernel/print.h>
#include <kernel/xarray.h>
#include <kernel/cmdline.h>

static irq_chip_t apic_chip;

static uintptr_t local_apic_address;
static volatile void *local_apic;
static xarray_t ioapic_list;

static intrnum_t allocate_vector(void) {
	static intrnum_t i = 64;
	return i++;
}

int have_apic(void) {
	if (kcmdline_have_opt("--disable-apic")) {
		return 0;
	}
	return acpi_find_table(ACPI_MADT_SIG) != NULL;
}

static uint32_t local_apic_read(uint16_t reg) {
	return mmio_read32(local_apic, reg);
}

static void local_apic_write(uint16_t reg, uint32_t value) {
	mmio_write32(local_apic, reg, value);
}

static uint32_t ioapic_read(ioapic_t *ioapic, uint8_t reg) {
	mmio_write32(ioapic->mmio, IOAPIC_REGSEL, reg);
	return mmio_read32(ioapic->mmio, IOAPIC_WIN);
}

static void ioapic_write(ioapic_t *ioapic, uint8_t reg, uint32_t value) {
	mmio_write32(ioapic->mmio, IOAPIC_REGSEL, reg);
	mmio_write32(ioapic->mmio, IOAPIC_WIN, value);
}

static uint64_t ioapic_read_redirection(ioapic_t *ioapic, size_t index) {
	size_t reg = IOAPIC_REG_REDTBL + index * 2;
	return ioapic_read(ioapic, reg) | ((uint64_t)ioapic_read(ioapic, reg + 1) << 32);
}

static void ioapic_write_redirection(ioapic_t *ioapic, size_t index, uint64_t value) {
	size_t reg = IOAPIC_REG_REDTBL + index * 2;
	ioapic_write(ioapic, reg, value & 0xffffffff);
	ioapic_write(ioapic, reg + 1, (value >> 32) & 0xffffffff);
}

static ioapic_t *get_ioapic_for_gsi(uint32_t gsi) {
	xarray_foreach (id, value, &ioapic_list) {
		(void)id;
		ioapic_t *ioapic = value;
		if (gsi >= ioapic->gsi_base && gsi <= ioapic->gsi_base + ioapic->redirections_count) {
			return ioapic;
		}
	}
	return NULL;
}

void init_apic(void) {
	acpi_madt_t *madt = acpi_find_table(ACPI_MADT_SIG);
	if (!madt) {
		kfail();
		kinfof("fail to get MADT table\n");
		halt();
	}

	// fun fact : to init the APIC you need to init the ... PIC
	init_pic();
	
	// then we mask all interruptions of the PIC
	out_byte(0x21, 0xff);
	out_byte(0xa1, 0xff);

	xarray_init(&ioapic_list);
	local_apic_address = madt->local_acpi_address;

	// got trough each entry
	uintptr_t current = (uintptr_t)madt + sizeof(acpi_madt_t);
	uintptr_t end     = (uintptr_t)madt + madt->sdt.length;
	while (current < end) {
		acpi_madt_entry_t *entry = (acpi_madt_entry_t *)current;
		switch (entry->type) {
		case ACPI_MADT_ENTRY_IOAPIC:;
			ioapic_t *ioapic           = kmalloc(sizeof(ioapic_t));
			ioapic->gsi_base           = entry->ioapic.gsi_base;
			ioapic->id                 = entry->ioapic.ioapic_id;
			ioapic->address            = entry->ioapic.address;
			ioapic->mmio               = mmio_map(ioapic->address, PAGE_SIZE);
			ioapic->redirections_count = ((ioapic_read(ioapic, IOAPIC_REG_VER) & IOAPIC_REDIRECTIONS_COUNT) >> IOAPIC_REDIRECTIONS_COUNT_SHIFT) + 1;

			xarray_set(&ioapic_list, ioapic->id, ioapic);
			kinfof("found io apic at address %p, gsi base %u, redirections count %zu\n", ioapic->address, ioapic->gsi_base, ioapic->redirections_count);
			break;
		case ACPI_MADT_ENTRY_LOCAL_APIC_ADDRESS_OVERRIDE:
			local_apic_address = entry->local_apic_address_override.address;
			break;
		}
		current += entry->length;
	}

	// setup irq objects
	xarray_foreach (id, value, &ioapic_list) {
		(void)id;
		ioapic_t *ioapic = value;
		for (size_t i=0; i<ioapic->redirections_count; i++) {
			irqnum_t gsi = ioapic->gsi_base + i;
			irq_t *irq = irq_allocate_object(gsi, gsi);
			// let the irq system allocate a vector for us
			irq_set_vector(irq, allocate_vector());
			irq_add_to_chip(&apic_chip, irq);
			kdebugf("gsi=%d vector=%d hwirq=%d\n", gsi, irq->vector, irq->hwirq);

			uint64_t redirection = ioapic_read_redirection(ioapic, i);
			// TODO : assign a cpu when we get SMP
			redirection &= ~(IOAPIC_VECTOR | IOAPIC_DESTINATION);
			redirection |= (irq->vector & IOAPIC_VECTOR) | IOAPIC_MASK;
			ioapic_write_redirection(ioapic, i, redirection);
		}
	}

	// repeat but apply redirections this time
	current = (uintptr_t)madt + sizeof(acpi_madt_t);
	while (current < end) {
		acpi_madt_entry_t *entry = (acpi_madt_entry_t *)current;
		switch (entry->type) {
		case ACPI_MADT_ENTRY_IOAPIC_INTERRUPT_OVERRIDE:
			kdebugf("redirection from %hhu to gsi %u\n", entry->ioapic_interrupt_override.irq_source, entry->ioapic_interrupt_override.gsi);
			// remove any irq which already has this hwirq
			irq_t *old_irq = irq_get_from_hwirq(&apic_chip, entry->ioapic_interrupt_override.irq_source);
			if (old_irq) {
				old_irq->hwirq = IRQ_NO_HWIRQ;
			}
			irq_t *irq = irq_get_from_irqnum(&apic_chip, entry->ioapic_interrupt_override.gsi);
			if (!irq) break;
			irq->hwirq = entry->ioapic_interrupt_override.irq_source;
			ioapic_t *ioapic = get_ioapic_for_gsi(entry->ioapic_interrupt_override.gsi);
			if (!ioapic) break;
			uint64_t redirection = ioapic_read_redirection(ioapic, entry->ioapic_interrupt_override.gsi - ioapic->gsi_base);
			switch (entry->ioapic_interrupt_override.flags & ACPI_MADT_ENTRY_INTERRUPT_POLARITY) {
			case ACPI_MADT_ENTRY_INTERRUPT_POLARITY_HIGH:
				redirection &= ~IOAPIC_PIN_POLARITY_LOW;
				break;
			case ACPI_MADT_ENTRY_INTERRUPT_POLARITY_LOW:
				redirection |= IOAPIC_PIN_POLARITY_LOW;
				break;
			}
			switch (entry->ioapic_interrupt_override.flags & ACPI_MADT_ENTRY_INTERRUPT_TRIGGER) {
			case ACPI_MADT_ENTRY_INTERRUPT_TRIGGER_EDGE:
				redirection &= ~IOAPIC_TRIGGER_MODE_LEVEL;
				break;
			case ACPI_MADT_ENTRY_INTERRUPT_TRIGGER_LEVEL:
				redirection |= IOAPIC_TRIGGER_MODE_LEVEL;
				break;
			}
			ioapic_write_redirection(ioapic, entry->ioapic_interrupt_override.gsi - ioapic->gsi_base, redirection);
			break;
		}
		current += entry->length;
	}

	// turn remaining vectors into msi
	for (;;) {
		intrnum_t vector = allocate_vector();
		if (vector > 255) break;
		irq_t *irq = irq_allocate_object(IRQ_MSI_START + vector, IRQ_NO_HWIRQ);
		if (!irq) break;
		irq_set_vector(irq, vector);
		irq_add_to_chip(&apic_chip, irq);
	}


	// tell the irq system we use apic
	main_irq_chip = &apic_chip;

	// enable local apic
	uint32_t apic_base_low;
	uint32_t apic_base_high;
	rdmsr(IA32_APIC_BASE, &apic_base_low, &apic_base_high);
	apic_base_low |= (1 << 11);
	wrmsr(IA32_APIC_BASE, apic_base_low, apic_base_high);

	kinfof("local apic address is %p\n", local_apic_address);
	local_apic = mmio_map(local_apic_address, 0x400);

	// we need to set the bit 8 of spurious interrupt vector to enable interrupts
	local_apic_write(LOCAL_APIC_REG_SPURIOUS, local_apic_read(LOCAL_APIC_REG_SPURIOUS) | 0x100);
}

static void apic_mask(irq_chip_t *irq_chip, irq_t *irq) {
	(void)irq_chip;
	ioapic_t *ioapic = get_ioapic_for_gsi(irq->irqnum);
	if (!ioapic) return;
	uint64_t redirection = ioapic_read_redirection(ioapic, irq->irqnum - ioapic->gsi_base);
	redirection |= IOAPIC_MASK;
	ioapic_write_redirection(ioapic, irq->irqnum - ioapic->gsi_base, redirection);
}

static void apic_unmask(irq_chip_t *irq_chip, irq_t *irq) {
	(void)irq_chip;
	ioapic_t *ioapic = get_ioapic_for_gsi(irq->irqnum);
	if (!ioapic) return;
	uint64_t redirection = ioapic_read_redirection(ioapic, irq->irqnum - ioapic->gsi_base);
	redirection &= ~IOAPIC_MASK;
	ioapic_write_redirection(ioapic, irq->irqnum - ioapic->gsi_base, redirection);
}

static void apic_eoi(irq_chip_t *irq_chip, irq_t *irq) {
	(void)irq_chip;
	(void)irq;
	local_apic_write(LOCAL_APIC_REG_EOI, 0);
}

static uintptr_t apic_msi_get_address(irq_chip_t *irq_chip, irq_t *irq) {
	(void)irq_chip;
	(void)irq;
	return local_apic_address;
}

static uint32_t apic_msi_get_data(irq_chip_t *irq_chip, irq_t *irq) {
	(void)irq_chip;
	uint32_t data = irq->vector & 0xff;
	return data;
}

static irq_chip_t apic_chip = {
	.name            = "APIC",
	.type            = IRQ_CHIP_APIC,
	.mask            = apic_mask,
	.unmask          = apic_unmask,
	.eoi             = apic_eoi,
	.msi_get_address = apic_msi_get_address,
	.msi_get_data    = apic_msi_get_data,
};
