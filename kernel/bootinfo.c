#include <kernel/bootinfo.h>
#include <kernel/print.h>

static bootinfo_t *bootinfo;

void bootinfo_set(bootinfo_t *new_bootinfo) {
	bootinfo = new_bootinfo;
}

void init_bootinfo(void) {
	// TODO : support other bootloaders
	init_limine();
    kinfof("using bootloader '%s'\n", bootinfo->name);
}

uintptr_t bootinfo_get_hhdm(void) {
	return bootinfo->hhdm;
}

uintptr_t bootinfo_get_kernel_paddr(void) {
	return bootinfo->kernel_paddr;
}

size_t bootinfo_memmap_get_entries_count(void) {
	return bootinfo->memmap_entries_count;
}

void bootinfo_memmap_get_entry(size_t index, bootinfo_memmap_entry_t *entry) {
	if (!bootinfo->memmap_get_entry) {
		kerrorf("unimplemented operation memmap_get_entry for bootloader '%s'\n", bootinfo->name);
		return;
	}
	bootinfo->memmap_get_entry(index, entry);
}
