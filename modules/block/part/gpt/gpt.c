#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/print.h>
#include <kernel/block.h>
#include <module/mbr.h>
#include <stdint.h>
#include <errno.h>

// module for GPT partions

#define GPT_ID 0xEE

typedef struct gpt_guid {
    uint32_t e1;
    uint16_t e2;
    uint16_t e3;
    uint16_t e4;
    uint8_t  e5[6];
} __attribute__((packed)) gpt_guid_t;

typedef struct gpt {
	char signature[8];
	uint32_t revision;
	uint32_t header_size;
	uint32_t checksum;
	uint32_t reserved;
	uint64_t lba_header;
	uint64_t lba_alt_header;
	uint64_t lba_start;
	uint64_t lba_end;
	gpt_guid_t guid;
	uint64_t lba_guid;
	uint32_t part_count;
	uint32_t part_ent_size;
	uint32_t checksum_part;
} __attribute__((packed)) gpt_header_t;

#define GPT_SIGNATURE "EFI PART"

typedef struct gpt_entry {
	gpt_guid_t type;
	gpt_guid_t guid;
	uint64_t lba_start;
	uint64_t lba_end;
	uint64_t attribute;
	char name[72];
} __attribute__((packed)) gpt_entry_t;

static void guid2str(gpt_guid_t *guid, char *buf, size_t buf_size) {
	int ptr = snprintf(buf, buf_size, "%08x-%04hx-%04hx-%04hx-", guid->e1, guid->e2, guid->e3, ((guid->e4 & 0xff) << 8) | ((guid->e4 >> 8) & 0xff));
	for (int i = 0; i < 6; i++) {
		ptr += sprintf(buf + ptr, "%02hhx", guid->e5[i]);
	}
}

static int gpt_probe(block_device_t *block_device) {
	mbr_table_t mbr;
	block_device_read(block_device, &mbr, 0, sizeof(mbr));
	if (mbr.signature != MBR_SIGNATURE) return 0;

	// check the mbr
	for (size_t i = 0; i < 4; i++) {
		if (mbr.entries[i].type == GPT_ID) {
			// we have a gpt !
			// return priority 2 so we are used over the mbr driver
			return 2;
		}
	}
	return 0;
}

static int gpt_attach(block_device_t *block_device) {
	gpt_header_t gpt;
	block_device_read(block_device, &gpt, block_device->sector_size, sizeof(gpt));
	if (memcmp(gpt.signature, GPT_SIGNATURE, sizeof(gpt.signature))) {
		return -EFTYPE;
	}
	//TODO : check the checksum
	
	gpt_guid_t guid;
	guid = gpt.guid;
	guid2str(&guid, block_device->uuid, sizeof(block_device->uuid));

	off_t offset = 2 * block_device->sector_size;
	for (size_t i = 0; i < gpt.part_count; i++, offset += gpt.part_ent_size) {
		gpt_entry_t entry;
		block_device_read(block_device, &entry, offset, sizeof(entry));

		// ignore empty partitions
		gpt_guid_t zero;
		memset(&zero, 0, sizeof(zero));
		if (!memcmp(&entry.type, &zero, sizeof(gpt_guid_t)))continue;

		char uuid[64];
		gpt_guid_t guid = entry.guid;
		guid2str(&guid, uuid, sizeof(uuid));

		char fs_uuid[64];
		gpt_guid_t fs_guid = entry.type;
		guid2str(&fs_guid, fs_uuid, sizeof(fs_uuid));

		block_device_add_partition(block_device, entry.lba_start * block_device->sector_size, (entry.lba_end - entry.lba_start) * block_device->sector_size, uuid, fs_uuid);
	}

	return 0;
}

static block_partition_driver_t gpt_driver = {
	.name = "gpt",
	.probe  = gpt_probe,
	.attach = gpt_attach,
};

int gpt_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	return block_partition_driver_register(&gpt_driver);
}

int gpt_fini(void) {
	return block_partition_driver_unregister(&gpt_driver);
}

kmodule_t module_meta = {
	.magic = MODULE_MAGIC,
	.init = gpt_init,
	.fini = gpt_fini,
	.author = "tayoky",
	.name = "gpt",
	.description = "partition driver for GPT",
	.license = "GPL 3",
};
