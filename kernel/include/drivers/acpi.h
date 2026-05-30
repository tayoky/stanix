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
	char oemi_table_id[8];
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

#define ACPI_RSDP_SIG "RSD PTR "

void acpi_set_rsdp(void *rsdp);
int acpi_sdt_verify(acpi_sdt_t *sdt, const char *name);
void *acpi_find_table(const char *name);
void init_acpi(void);

#endif
