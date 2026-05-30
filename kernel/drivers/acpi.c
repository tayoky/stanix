#include <kernel/acpi.h>
#include <kernel/print.h>
#include <kernel/string.h>

static acpi_xsdp_t *rsdp;

void acpi_set_rsdp(void *new_rsdp) {
	rsdp = new_rsdp;
}

void init_acpi(void) {
	kstatusf("init acpi ...");
	if (!rsdp) {
		kfail();
		kinfof("no rsdp was provided (the machine or the bootloader probably don't support ACPI)\n");
		return;
	}

	// signature
	if (memcmp(rsdp->signature, ACPI_RSDP_SIG, 8)) {
		kfail();
		kinfof("invalid signature for rsdp\n");
		return;
	}

	// checksum
	uint8_t sum   = 0;
	uint8_t *data = (uint8_t *)rsdp;
	for (size_t i = 0; i < 20; i++) {
		sum += data[i];
	}
	if (sum != 0) {
		kfail();
		kinfof("invalid checksum for rsdp (corrupted rsdp ?)\n");
		return;
	}

	switch (rsdp->revision) {
	case 0:
		// version1
		break;
	case 1:
		kfail();
		kinfof("invalid rsdp revision %hhu\n", rsdp->revision);
		return;
	default:
		kwarningf("unknow rsdp revision %hhu, parsing as resvison 2\n", rsdp->revision);
		// fallthrough
	case 2:
		// version 2
		// second checksum
		sum = 0;
		for (size_t i = 0; i < rsdp->length; i++) {
			sum += data[i];
		}
		if (sum != 0) {
			kfail();
			kinfof("invalid extended checksum for rsdp (corrupted rsdp ?)\n");
		}
		break;
	}
	kok();
	kinfof("rsdp revision : %hhu\n", rsdp->revision);
    kinfof("OEM is %.6s\n", rsdp->oem_id);
}
