#ifndef _MODULE_PART_H
#define _MODULE_PART_H

#include <stdint.h>

typedef struct gpt_guid {
    uint32_t e1;
    uint16_t e2;
    uint16_t e3;
    uint16_t e4;
    uint8_t  e5[6];
} gpd_guid_t;

typedef struct gpt_info {
    gpt_guid_t disk_uuid;
	gpt_guid_t part_uuid;
    gpt_guid_t type;
} gpt_info_t;

typedef struct mbr_info {
    uint32_t disk_uuid;
    uint8_t type;
} mbr_info_t;

typedef struct part_info {
    int type;
    union {
        gpt_info_t gpt;
        mbr_info_t mbr;
    };
	size_t offset;
	size_t size;
	char padding[128];
} part_info_t;

#define PART_TYPE_MBR 1
#define PART_TYPE_GPT 2

#define PART_GET_INFO       19000
#define PART_OPEN_DISK      19001

#endif
