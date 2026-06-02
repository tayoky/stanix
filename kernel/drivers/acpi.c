#include <kernel/acpi.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/string.h>
#include <kernel/cmdline.h>
#include <errno.h>

static acpi_xsdp_t *rsdp;
static acpi_rsdt_t *rsdt;
static acpi_xsdt_t *xsdt;

void acpi_set_rsdp(void *new_rsdp) {
	rsdp = new_rsdp;
}

int acpi_sdt_verify(acpi_sdt_t *sdt, const char *name) {
	if (memcmp(sdt->signature, name, sizeof(sdt->signature))) {
		// invalid name
        kwarningf("invalid name for %s (corrupted %s ?)\n", name);
		return -EINVAL;
	}
	uint8_t sum   = 0;
	uint8_t *data = (uint8_t *)sdt;
	for (size_t i = 0; i < sdt->length; i++) {
		sum += data[i];
	}
	if (sum != 0) {
		// invalid checksum
		return -EINVAL;
	}
	return 0;
}

void *acpi_find_table(const char *name) {
	if (rsdt) {
		size_t entries_count = (rsdt->sdt.length - sizeof(acpi_sdt_t)) / sizeof(uint32_t);
		for (size_t i = 0; i < entries_count; i++) {
			acpi_sdt_t *current = (acpi_sdt_t *)(rsdt->entries[i] + kernel->hhdm);
			if (!memcmp(current->signature, name, 4)) {
				if (acpi_sdt_verify(current, name) < 0) return NULL;
				return current;
			}
		}
        return NULL;
	} else if (xsdt) {
		size_t entries_count = (xsdt->sdt.length - sizeof(acpi_sdt_t)) / sizeof(uint64_t);
		for (size_t i = 0; i < entries_count; i++) {
			acpi_sdt_t *current = (acpi_sdt_t *)(xsdt->entries[i] + kernel->hhdm);
			if (!strcmp(current->signature, name)) {
				if (acpi_sdt_verify(current, name) < 0) return NULL;
				return current;
			}
		}
        return NULL;
	} else {
		return NULL;
	}
}

void init_acpi(void) {
	kstatusf("init acpi ...");
	if (kcmdline_have_opt("--disable-acpi")) {
		kfail();
		kinfof("acpi is disabled\n");
		return;
	}
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
		rsdt = (acpi_rsdt_t *)(rsdp->rsdt_address + kernel->hhdm);
        if (acpi_sdt_verify(&rsdt->sdt, "RSDT") < 0) {
            kfail();
            kinfof("invalid rsdt\n");
            rsdt = NULL;
            return;
        }
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
		xsdt = (acpi_xsdt_t *)(rsdp->xsdt_address + kernel->hhdm);
        if (acpi_sdt_verify(&xsdt->sdt, "XSDT") < 0) {
            kfail();
            kinfof("invalid xsdt\n");
            xsdt = NULL;
            return;
        }
		break;
	}
	kok();
	kinfof("rsdp revision : %hhu\n", rsdp->revision);
	kinfof("OEM is %.6s\n", rsdp->oem_id);
}
