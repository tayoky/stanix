#include <kernel/module.h>
#include <kernel/kheap.h>
#include <kernel/bus.h>
#include <module/ata.h>

static void ata2str(char *dest, const char *atastr, size_t size) {
	for (size_t i=0; i<size; i+=2) {
		dest[i + 1] = atastr[i];
		dest[i]     = atastr[i + 1];
	}
	for (size_t i=size-1; i>0; i--) {
		if (dest[i] != ' ') {
			break;
		}
		dest[i] = '\0';
	}
	// always null terminate just in case
	dest[size] = '\0';
}

void ata_parse_common_ident(ata_common_ident_t *common_ident, ata_ident_t *ident);
	common_ident->sectors_count = ident->command_sets & (1 << 26) ? ident->sectors_lba48 : ident->sectors;
	ata2str(common_ident->model,    ident->model,    sizeof(ident->model));
	ata2str(common_ident->firmware, ident->firmware, sizeof(ident->firmware));
	ata2str(common_ident->serial,   ident->serial,   sizeof(ident->serial));
	common_ident->command_sets = ident->command_sets;

	kdebugf("model : %s command sets : %x support LBA48 : %s max LBA : %ld\n", common_ident->model, common_ident->command_sets, common_ident->command_sets & (1 << 26) ? "true" : "false", common_ident->sectors_count);

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
