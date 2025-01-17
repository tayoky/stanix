#ifndef BITMAP_H
#define BITMAP_H
#include <stdint.h>


typedef struct bitmap_struct{
	/// @brief size in uin64_t count
	uint64_t size;
	/// @brief the first free page (-1 if not caculated)
	uint64_t last_allocated;
	uint64_t page_count;
	uint64_t used_page_count;
	uint64_t *data;
}bitmap_meta;


/// @brief init the bitmap
void init_bitmap();

/// @brief find an free page and allocate it
/// @param bitmap an pointer to the bitmap
/// @return the address of the page (multiply it by 0x1000 to get it's real address)
uint64_t allocate_page(bitmap_meta *bitmap);

/// @brief mark an page as allocated
/// @param bitmap an pointer to the bitmap
/// @param page the page to mark as allocated
void set_allocted_page(bitmap_meta *bitmap,uint64_t page);

/// @brief free an specified page
/// @param bitmap an pointer to the bitmap
/// @param page the page to free
void free_page(bitmap_meta *bitmap,uint64_t page);


#endif