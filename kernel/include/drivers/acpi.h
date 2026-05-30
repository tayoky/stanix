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

#define ACPI_RSDP_SIG "RSD PTR "

void acpi_set_rsdp(void *rsdp);
void init_acpi(void);

#endif
