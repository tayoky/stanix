#include <kernel/module.h>
#include <kernel/kheap.h>
#include <kernel/bus.h>
#include <module/ata.h>

void ata_parse_common_ident(ata_common_ident_t *common_ident, ata_ident_t *ident);
	ata_device_t *device = kmalloc(sizeof(ata_device_t));
	if (!device) return -ENOMEM;
	memset(device, 0, sizeof(ata_device_t));
	device->signature = ata_get_signature(devnode);

	device->sectors_count = ident.command_sets & (1 << 26) ? ident.sectors_lba48 : ident.sectors;
	for (size_t i = 0; i < sizeof(ident.model); i += 2) {
		device->model[i + 1] = ident.model[i];
		device->model[i]     = ident.model[i + 1];
	}
	for (size_t i = sizeof(device->model) - 1; i > 0 && device->model[i] == ' '; i--) {
		device->model[i] = '\0';
	}
	device->command_sets = ident.command_sets;

	kdebugf("model : %s command sets : %x support LBA48 : %s max LBA : %ld\n", device->model, device->command_sets, device->command_sets & (1 << 26) ? "true" : "false", device->sectors_count);

	return 0;
}

int libata_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	EXPORT(ata_parse_common_ident);
	return 0;
}

int libata_fini(void) {
	UNEXPORT(ata_parse_common_ident);
	return 0;
}

kmodule_t module_meta = {
	.magic       = MODULE_MAGIC,
	.init        = libata_init,
	.fini        = libata_fini,
	.author      = "tayoky",
	.name        = "libata",
	.description = "common ATA utils",
	.license     = "GPL 3",
};
