#ifndef BITMAP_H
#define BITMAP_H

struct kernel_table_struct;
void init_bitmap(struct kernel_table_struct *kernel);
uint64_t allocate_page();
void free_page(uint64_t page);

#endif