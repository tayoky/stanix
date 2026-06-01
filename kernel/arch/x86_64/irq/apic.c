#include <kernel/acpi.h>
#include <kernel/arch.h>
#include <kernel/irq.h>
#include <kernel/kernel.h>
#include <kernel/kheap.h>
#include <kernel/mmio.h>
#include <kernel/print.h>
#include <kernel/xarray.h>

static irq_chip_t apic_chip;

static uintptr_t local_apic_address;
static volatile void *local_apic;
static xarray_t ioapic_list;
static xarray_t hirq2gsi;

int have_apic(void) {
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
	ioapic_write(ioapic, reg, (value >> 32) & 0xffffffff);
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
	// this mask all interruptions of the PIC
	init_pic();

	xarray_init(&ioapic_list);
	xarray_init(&hirq2gsi);
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
			ioapic->redirections_count = (ioapic_read(ioapic, IOAPIC_REG_VER) & IOAPIC_REDIRECTIONS_COUNT) >> IOAPIC_REDIRECTIONS_COUNT_SHIFT;

			xarray_set(&ioapic_list, ioapic->id, ioapic);
			kdebugf("found io apic at address %p, gsi base %u, redirections count %zu\n", ioapic->address, ioapic->gsi_base, ioapic->redirections_count);
			break;
		case ACPI_MADT_ENTRY_LOCAL_APIC_ADDRESS_OVERRIDE:
			local_apic_address = entry->local_apic_address_override.address;
			break;
		}
		current += entry->length;
	}

	// repeat but apply redirections this time
	current = (uintptr_t)madt + sizeof(acpi_madt_t);
	while (current < end) {
		acpi_madt_entry_t *entry = (acpi_madt_entry_t *)current;
		switch (entry->type) {
		case ACPI_MADT_ENTRY_IOAPIC_INTERRUPT_OVERRIDE:
			// we store values in xarray multiplied by 2 since we need them to be 2 aligned
			xarray_set(&hirq2gsi, entry->ioapic_interrupt_override.irq_source, (void *)(uintptr_t)(entry->ioapic_interrupt_override.gsi * 2));
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


	// tell the irq system we use apic
	irq_chip = &apic_chip;

	kinfof("local apic address is %p\n", local_apic_address);
	local_apic = mmio_map(local_apic_address, 0x400);

	// we need to set the bit 8 of spurious interrupt vector to enable interrupts
	local_apic_write(LOCAL_APIC_REG_SPURIOUS, local_apic_read(LOCAL_APIC_REG_SPURIOUS) | 0x100);
}

static void apic_mask(irqnum_t gsi) {
	ioapic_t *ioapic = get_ioapic_for_gsi(gsi);
	if (!ioapic) return;
	uint64_t redirection = ioapic_read_redirection(ioapic, gsi - ioapic->gsi_base);
	redirection |= IOAPIC_MASK;
	ioapic_write_redirection(ioapic, gsi - ioapic->gsi_base, redirection);
}

static void apic_unmask(irqnum_t gsi) {
	ioapic_t *ioapic = get_ioapic_for_gsi(gsi);
	if (!ioapic) return;
	uint64_t redirection = ioapic_read_redirection(ioapic, gsi - ioapic->gsi_base);
	redirection &= ~IOAPIC_MASK;
	ioapic_write_redirection(ioapic, gsi - ioapic->gsi_base, redirection);
}

static void apic_eoi(irqnum_t gsi) {
	(void)gsi;
	local_apic_write(LOCAL_APIC_REG_EOI, 0);
}

static irqnum_t apic_hirq2irq(int hirq) {
	// we store values in xarray multiplied by 2 since we need them to be 2 aligned
	uintptr_t val = (uintptr_t)xarray_get(&hirq2gsi, hirq);
	return val / 2;
}

static irq_chip_t apic_chip = {
	.name     = "APIC",
	.type     = IRQ_CHIP_APIC,
	.mask     = apic_mask,
	.unmask   = apic_unmask,
	.eoi      = apic_eoi,
	.hirq2irq = apic_hirq2irq,
};
