#ifndef KERNEL_ACPI_H
#define KERNEL_ACPI_H

#include <stdint.h>

// struct picked from osdev wiki

// struct for revision 0 (version 1.0)
typedef struct acpi_rsdp {
	char signature[8];
	uint8_t checksum;
	char oem_id[6];
	uint8_t revision;
	uint32_t rsdt_address;
} __attribute__((packed)) acpi_rsdp_t;

// struct for revision 2 (version 2.0)
typedef struct acpi_xsdp {
	char signature[8];
	uint8_t checksum;
	char oem_id[6];
	uint8_t revision;
	uint32_t rsdt_address; // deprecated since version 2.0

	uint32_t length;
	uint64_t xsdt_address;
	uint8_t extended_checksum;
	uint8_t reserved[3];
} __attribute__((packed)) acpi_xsdp_t;

typedef struct acpi_sdt {
	char signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char oem_id[6];
	char oem_table_id[8];
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_t;

typedef struct acpi_rsdt {
	acpi_sdt_t sdt;
	uint32_t entries[];
} __attribute__((packed)) acpi_rsdt_t;

typedef struct acpi_xsdt {
	acpi_sdt_t sdt;
	uint64_t entries[];
} __attribute__((packed)) acpi_xsdt_t;

typedef struct acpi_madt {
	acpi_sdt_t sdt;
	uint32_t local_acpi_address;
	uint32_t flags;
} __attribute__((packed)) acpi_madt_t;

#define ACPI_MADT_FLAG_DUAL_PIC 0x1

typedef struct acpi_madt_entry {
	uint8_t type;
	uint8_t length;
	union {
		struct {
			uint8_t processor_id;
			uint8_t apic_id;
			uint32_t flags;
		} local_apic;
		struct {
			uint8_t ioapic_id;
			uint8_t reserved;
			uint32_t address;
			uint32_t gsi_base;
		} ioapic;
		struct {
			uint8_t bus_source;
			uint8_t irq_source;
			uint32_t gsi;
			uint16_t flags;
		} ioapic_interrupt_override;
		struct {
			uint8_t nmi_source;
			uint8_t reserved;
			uint16_t flags;
			uint32_t gsi;
		} ioapic_nmi;
		struct {
			uint8_t processor_id;
			uint16_t flags;
			uint8_t lint;
		} local_apic_nmi;
		struct {
			uint16_t reserved;
			uint64_t address;
		} local_apic_address_override;
		struct {
			uint16_t reserved;
			uint32_t apic_id;
			uint32_t flags;
			uint32_t processor_id;
		} local_x2apic;
	};
} __attribute__((packed)) acpi_madt_entry_t;

#define ACPI_MADT_ENTRY_LOCAL_APIC                  0x0 // Local APIC
#define ACPI_MADT_ENTRY_IOAPIC                      0x1 // IO APIC
#define ACPI_MADT_ENTRY_IOAPIC_INTERRUPT_OVERRIDE   0x2 // IO APIC Interrupt Source Override
#define ACPI_MADT_ENTRY_IOAPIC_NMI                  0x3 // IO APIC non-maskable interrupts
#define ACPI_MADT_ENTRY_LOCAL_APIC_NMI              0x4 // Local APIC non-maskable interrupts
#define ACPI_MADT_ENTRY_LOCAL_APIC_ADDRESS_OVERRIDE 0x5 // Local APIC address override
#define ACPI_MADT_ENTRY_LOCAL_X2APIC                0x9 // Local x2APIC

#define ACPI_MADT_ENTRY_LOCAL_APIC_ENABLED 0x1 // processor enabeld
#define ACPI_MADT_ENTRY_LOCAL_APIC_ONLINE  0x2 // online capable

#define ACPI_MADT_ENTRY_INTERRUPT_POLARITY          0x3
#define ACPI_MADT_ENTRY_INTERRUPT_POLARITY_NONE     0x0
#define ACPI_MADT_ENTRY_INTERRUPT_POLARITY_HIGH     0x1
#define ACPI_MADT_ENTRY_INTERRUPT_POLARITY_RESERVED 0x2
#define ACPI_MADT_ENTRY_INTERRUPT_POLARITY_LOW      0x3

#define ACPI_MADT_ENTRY_INTERRUPT_TRIGGER          0xc
#define ACPI_MADT_ENTRY_INTERRUPT_TRIGGER_NONE     0x00
#define ACPI_MADT_ENTRY_INTERRUPT_TRIGGER_EDGE     0x4
#define ACPI_MADT_ENTRY_INTERRUPT_TRIGGER_RESERVED 0x8
#define ACPI_MADT_ENTRY_INTERRUPT_TRIGGER_LEVEL    0xc


#define ACPI_RSDP_SIG "RSD PTR "
#define ACPI_MADT_SIG "APIC"

void acpi_set_rsdp(void *rsdp);
int acpi_sdt_verify(acpi_sdt_t *sdt, const char *name);
void *acpi_find_table(const char *name);
void init_acpi(void);

#endif
