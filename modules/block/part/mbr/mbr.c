#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/print.h>
#include <kernel/block.h>
#include <module/mbr.h>
#include <stdint.h>

// module for MBR partions

static int mbr_probe(block_device_t *block_device) {
	mbr_table_t mbr;
	block_device_read(block_device, &mbr, 0, sizeof(mbr));

	return mbr.signature == MBR_SIGNATURE;
}

static int mbr_attach(block_device_t *block_device) {
	mbr_table_t mbr;
	block_device_read(block_device, &mbr, 0, sizeof(mbr));

	snprintf(block_device->uuid, sizeof(block_device->uuid), "%x", mbr.uuid);

	for (size_t i = 0; i < 4; i++) {
		if (mbr.entries[i].sectors_count == 0) continue;
		char fs_uuid[8];
		snprintf(fs_uuid, sizeof(fs_uuid), "%hhx", mbr.entries[i].type);
		block_device_add_partition(block_device, mbr.entries[i].lba_start * block_device->sector_size, mbr.entries[i].sectors_count * block_device->sector_size, NULL, fs_uuid);
	}

	return 0;
}

static block_partition_driver_t mbr_driver = {
	.name = "mbr",
	.probe  = mbr_probe,
	.attach = mbr_attach,
};

int mbr_init(int argc, char **argv) {
	(void)argc;
	(void)argv;

	return block_partition_driver_register(&mbr_driver);
}

int mbr_fini(void) {
	return block_partition_driver_unregister(&mbr_driver);
}

kmodule_t module_meta = {
	.magic = MODULE_MAGIC,
	.init = mbr_init,
	.fini = mbr_fini,
	.author = "tayoky",
	.name = "mbr",
	.description = "partition driver for MBR",
	.license = "GPL 3",
};
