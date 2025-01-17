#ifndef BOOTINFO_H
#define BOOTINFO_H

#include "limine.h"

typedef struct {
	struct limine_boot_time_response *boot_time_response;
}bootinfo_table;

void get_bootinfo(void);

#endif