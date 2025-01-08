#include "bitmap.h"
#include "kernel.h"
#include "print.h"
#include "limine.h"
#include "panic.h"
#include "page.h"

//TODO add out of bound check 

void set_allocted_page(bitmap_meta *bitmap,uint64_t page){
	//find the good uint64_t
	uint64_t index = page / 64;
	uint64_t bit = 1 << (page % 64);
	bitmap->data[index] |= bit;
}

void free_page(bitmap_meta *bitmap,uint64_t page){
	//if the last allocated if after
	//then set the previous page
	if(page < bitmap->last_allocated && page > 0){
		bitmap->last_allocated = page;
	}

	//find the good uint64_t
	uint64_t index = page / 64;
	uint64_t bit = 1 << (page % 64);
	bitmap->data[index] &= ~bit;
}

uint64_t allocate_page(bitmap_meta *bitmap){
	//start at the last allocated uint64_t if caculated
	uint64_t index = 0;
	uint64_t bit = 1;
	if(bitmap->last_allocated != (uint64_t)-1){
		index = bitmap->last_allocated / 64;
	}

	while (bitmap->data[index] == (uint64_t)-1){
		index++;
		if(index >= bitmap->size){
			//OUT OF MEMORY
			regs registers;
			registers.cr2 = 0;
			panic("out of memory",registers);
		}
	}

	//now find the bit
	while (bitmap->data[index] & (1 << bit))
	{
		bit ++;
		if(bit >= 64){
			//should never happen
			regs registers;
			registers.cr2 = 0;
			panic("weird bug",registers);
		}
	}

	//mark it as allocated
	bitmap->data[index] |= 1<< bit;

	//now translate to a page number
	uint64_t page = index * 64 + bit;

	//set the last allocated acceleration
	bitmap->last_allocated = page;

	return page;
}

void init_bitmap(kernel_table *kernel){
	kstatus("init memory bitmap ...");

	//create an bitmap of the correspondig size
	//each unint64_t can store state for 64 page of 4KB each so 65536 bytes
	kernel->bitmap.size = kernel->total_memory/65536+1;

	//find a good segment we can use
	uint64_t selected_seg=0;

	while (kernel->memmap->entries[selected_seg]->type != LIMINE_MEMMAP_USABLE  || kernel->memmap->entries[selected_seg]->length < kernel->bitmap.size * sizeof(uint64_t) ){
		selected_seg++;
		if(selected_seg >= kernel->memmap->entry_count){
			kfail();
			kstatus("this is a vital step in init the kernel\n");
			kstatus("can't boot\n");
			kdebugf("bitmap size : %lu\n",kernel->bitmap.size);
			regs registers ={
				.cr2 = 0
			};
			panic("can't init bitmap",registers);
		}
	}

	//stupid (void *) to make compiler happy
	kernel->bitmap.data = (void *)kernel->memmap->entries[selected_seg]->base + kernel->hhdm;

	kdebugf("use segment %lu\n",selected_seg);

	//fill the bitmap with 1
	for(uint64_t index = 0;index < kernel->bitmap.size;index++){
		kernel->bitmap.data[index] = (uint64_t)-1;
	}

	//get all usable segment
	//and set it as free
	for (uint64_t index = 0; index < kernel->memmap->entry_count; index++){
		if(kernel->memmap->entries[index]->type != LIMINE_MEMMAP_USABLE)
			continue;
		uint64_t addr = kernel->memmap->entries[index]->base / PAGE_SIZE;
		uint64_t end = addr + (kernel->memmap->entries[index]->length / PAGE_SIZE);
		while (addr < end){
			free_page(&kernel->bitmap,addr);
			addr++;
		}
	}

	//set the pages used by the bimtap as used
	uint64_t bitmap_start_page = ((uint64_t)kernel->bitmap.data - kernel->hhdm)/PAGE_SIZE;
	for (uint64_t addr= 0; addr < kernel->bitmap.size; addr++)
	{
		set_allocted_page(&kernel->bitmap,bitmap_start_page + addr);
	}

	//set the accelaration as not caculated
	kernel->bitmap.last_allocated = (uint64_t)-1;
	
	kok();
}