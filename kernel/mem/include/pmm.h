#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>
#include <sys/type.h>

typedef struct PMM_entry_struct {
	struct PMM_entry_struct *next;
	size_t size; //size of the chunk in page
} PMM_entry;

/// @brief init the PMM
void init_PMM();

/// @brief find an free page and allocate it
/// @return the address of the page (multiply it by 0x1000 to get it's real address)
uintptr_t pmm_allocate_page();

/// @brief mark an page as allocated
/// @param page the page to mark as allocated
void pmm_free_page(uintptr_t page);



#endif