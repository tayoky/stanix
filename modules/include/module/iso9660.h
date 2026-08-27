#ifndef MODULE_ISO9660_H
#define MODULE_ISO9660_H

#include <kernel/endian.h>
#include <stdint.h>

typedef struct iso9660_le_be_uint16 {
	le_uint16_t le;
	be_uint16_t be;
} __attribute__((packed)) iso9660_le_be_uint16_t;

typedef struct iso9660_le_be_uint32 {
	le_uint16_t le;
	be_uint16_t be;
} __attribute__((packed)) iso9660_le_be_uint32_t;

typedef struct iso9660_time {
	char year[4];
	char month[2];
	char day[2];
	char hour[2];
	char minute[2];
	char second[2];
	char hundredth[2];
	int8_t timezone;
} __attribute__((packed)) iso9660_time_t;

typedef struct iso9660_small_time {
	uint8_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	uint8_t timezone;
} __attribute__((packed)) iso9660_small_time_t;

typedef struct iso9660_dentry {
	int8_t length;
	int8_t extended_attributes_length;
	iso9660_le_be_uint32_t lba;
	iso9660_le_be_uint32_t data_length;
	iso9660_small_time_t time;
	uint8_t flags;
	int8_t file_unit_size;
	int8_t interleave_gap_size;
	iso9660_le_be_uint16_t volume_sequence_number;
	int8_t filename_length;
	char file_identifier[];
} __attribute__((packed)) iso9660_dentry_t;

#define ISO9660_DENTRY_FLAG_HIDDEN              0x01
#define ISO9660_DENTRY_FLAG_DIRECTORY           0x02
#define ISO9660_DENTRY_FLAG_ASSOCIATED          0x04
#define ISO9660_DENTRY_FLAG_EXTENDED_ATTRIBUTED 0x08
#define ISO9660_DENTRY_FLAG_UNIX_DATA           0x10
#define ISO9660_DENTRY_FLAG_NOT_FINAL           0x80

typedef struct iso9660_boot_record {
	char boot_system_identifier[32];
	char boot_identifier[32];
} __attribute__((packed)) iso9660_boot_record_t;

typedef struct iso9660_primary_volume {
	uint8_t unused2;
	char system_identifier[32];
	char volume_identifier[32];
	uint8_t unused1[8];
	iso9660_le_be_uint32_t volume_size;
	uint8_t unused2[32];
	iso9660_le_be_uint16_t volume_set_size;
	iso9660_le_be_uint16_t volume_sequence_number;
	iso9660_le_be_uint16_t logical_block_size;
	iso9660_le_be_uint32_t path_table_size;
	le_int32_t lpath_table; 
	le_int32_t optional_lpath_table; 
	be_int32_t mpath_table; 
	be_int32_t optional_mpath_table;
	char root_dentry[34];
	char volume_set_identifier[128];
	char publisher_identifier[128];
	char data_preparer_identifier[128];
	char application_identifier[128];
	char copyright_file_identifier[37];
	char abstract_file_identifier[37];
	char bibliographic_file_identifier[37];
	iso9660_time_t creation_time;
	iso9660_time_t modification_time;
	iso9660_time_t expiration_time;
	iso9660_time_t effective_time;
	int8_t file_structure_version;
	uint8_t unused3;
	uint8_t application_used[512];
} __attribute__((packed)) iso9660_primary_volume_t;

typedef struct iso9660_volume_descriptor {
	int8_t type;
	char identifier[5];
	int8_t version;
	union __attribute__((packed)) {
		uint8_t data[2041];
		iso9660_boot_record_t boot_record;
		iso9660_primary_volume_t primary_volume;
	};
} __attribute__((packed)) iso9660_volume_descriptor_t;

#define ISO9660_VOLUME_DESCRIPTOR_BOOT_RECORD    0x00
#define ISO9660_VOLUME_DESCRIPTOR_PRIMARY        0x01
#define ISO9660_VOLUME_DESCRIPTOR_SUPPLEMENTARY  0x02
#define ISO9660_VOLUME_DESCRIPTOR_PARTITION      0x03
#define ISO9660_VOLUME_DESCRIPTOR_SET_TERMINATOR 0xff

#endif
