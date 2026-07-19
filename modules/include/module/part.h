#ifndef _MODULE_PART_H
#define _MODULE_PART_H

#include <stdint.h>

struct gpt_guid {
    uint32_t e1;
    uint16_t e2;
    uint16_t e3;
    uint16_t e4;
    uint8_t  e5[6];
};

struct gpt_info {
    struct gpt_guid disk_uuid;
    struct gpt_guid part_uuid;
    struct gpt_guid type;
};

struct mbr_info {
    uint32_t disk_uuid;
    uint8_t type;
};

struct part_info {
    int type;
    union {
        struct gpt_info gpt;
        struct mbr_info mbr;
    };
};

#define PART_TYPE_MBR 1
#define PART_TYPE_GPT 2

#define I_PART_GET_INFO       1193
#define IOCTL_PART_GET_INFO   I_PART_GET_INFO


#endif