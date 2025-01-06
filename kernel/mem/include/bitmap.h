#ifndef BITMAP_H
#define BITMAP_H
#include <stdint.h>

struct kernel_table_struct;

/// @brief init the bitmap
void init_bitmap(struct kernel_table_struct *kernel);

/// @brief find an free page and allocate it
/// @param bitmap an pointer to the bitmap
/// @param lenght the lenght of the bitmap (in uint64_t count)
/// @return the address of the page (multiply it by 0x1000 to get it's real address)
uint64_t allocate_page(uint64_t *bitmap,uint64_t lenght);

/// @brief mark an page as allocated
/// @param bitmap an pointer to the bitmap
/// @param page the page to mark as allocated
void set_allocted_page(uint64_t *bitmap,uint64_t page);

/// @brief free an specified page
/// @param bitmap an pointer to the bitmap
/// @param page the page to free
void free_page(uint64_t *bitmap,uint64_t page);


#endif