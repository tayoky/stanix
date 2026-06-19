#ifndef KERNEL_BOOTINFO_H
#define KERNEL_BOOTINFO_H

#include <stdint.h>
#include <stddef.h>

typedef struct bootinfo_memmap_entry {
	uintptr_t start;
	size_t size;
	int type;
} bootinfo_memmap_entry_t;

#define MEMMAP_USABLE      0
#define MEMMAP_RESERVED    1
#define MEMMAP_ACPI        2
#define MEMMAP_BAD         3
#define MEMMAP_BOOTLOADER  4
#define MEMMAP_KERNEL      5
#define MEMMAP_FRAMEBUFFER 6

typedef struct bootinfo_initrd {
    void *start;
    size_t size;
} bootinfo_initrd_t;

typedef struct bootinfo {
    const char *name;
    uintptr_t kernel_paddr;
    uintptr_t hhdm;
    bootinfo_initrd_t initrd;
    size_t memmap_entries_count;
    void (*memmap_get_entry)(size_t index, bootinfo_memmap_entry_t *entry);
} bootinfo_t;

void init_bootinfo(void);
void init_limine(void);
void bootinfo_set(bootinfo_t *new_bootinfo);
uintptr_t bootinfo_get_hhdm(void);
uintptr_t bootinfo_get_kernel_paddr(void);
bootinfo_initrd_t *bootinfo_get_initrd(void);
size_t bootinfo_memmap_get_entries_count(void);
void bootinfo_memmap_get_entry(size_t index, bootinfo_memmap_entry_t *entry);

#endif
