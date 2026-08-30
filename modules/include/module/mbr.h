#ifndef MODULE_MBR_H
#define MODULE_MBR_H

typedef struct mbr_entry {
	uint8_t attribute;
	char chs_start[3];
	uint8_t type;
	char chs_end[3];
	uint32_t lba_start;
	uint32_t sectors_count;
} __attribute__((packed)) mbr_entry_t;

typedef struct mbr {
	char bootstrap[440];
	uint32_t uuid;
	uint16_t reserved;
	mbr_entry_t entries[4];
	uint16_t signature;
} __attribute__((packed)) mbr_table_t;

#define MBR_SIGNATURE 0xaa55

#endif
