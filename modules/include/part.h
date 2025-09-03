#ifndef _MODULE_PART_H
#define _MODULE_PART_H

#include <stdint.h>

struct gpt_uuid {
    uint64_t disk_uuid[2];
    uint64_t part_uuid[2];
    uint64_t type[2];
};

struct mbr_uuid {
    uint32_t disk_uuid;
    uint8_t type;
};

struct part_info {
    int type;
    union {
        struct gpt_uuid gpt;
        struct mbr_uuid mbr;
    };
};

#define PART_TYPE_MBR 1
#define PART_TYPE_GPT 2

#define I_PART_GET_INFO       1193
#define IOCTL_PART_GET_INFO   I_PART_GET_INFO


#endif