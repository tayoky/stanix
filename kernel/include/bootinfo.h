#ifndef KERNEL_BOOTINFO_H
#define KERNEL_BOOTINFO_H

#include <kernel/limine.h>

typedef struct {
	struct limine_boot_time_response *boot_time_response;
} bootinfo_table;

void get_bootinfo(void);

#endif
