#ifndef BOOTINFO_H
#define BOOTINFO_H

#include "limine.h"

typedef struct {
	struct limine_boot_time_response *boot_time_response;
}bootinfo_table;

struct kernel_table_struct;
void get_bootinfo(struct kernel_table_struct *kernel);

#endif