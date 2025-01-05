#include "bitmap.h"
#include "kernel.h"
#include "print.h"
#include "limine.h"
#include "panic.h"

//TODO add out of bound check for free_page

void free_page(uint64_t *bitmap,uint64_t page){
	//find the good uint64_t
	uint64_t index = page / 64;
	uint64_t bit = 1 << (page % 64);
	bitmap[index] &= ~bit;
}

void init_bitmap(kernel_table *kernel){
	kstatus("init memory bitmap ...");

	//create an bitmap of the correspondig size
	//each unint64_t can store state for 64 page of 4KB each so 65536 bytes
	kernel->bitmap_size = kernel->total_memory/65536+1;

	//find a good segment we can use
	uint64_t i=0;
	while (kernel->memmap->entries[i]->type != LIMINE_MEMMAP_USABLE  || kernel->memmap->entries[i]->length < kernel->bitmap_size * sizeof(uint64_t) ){
		i++;
		if(i >= kernel->memmap->entry_count){
			kfail();
			kstatus("this is a vital step in init the kernel\n");
			kstatus("can't boot\n");
			kdebugf("bitmap size : %lu\n",kernel->bitmap_size);
			regs registers ={
				.cr2 = 0
			};
			panic("can't init bitmap",registers);
		}
	}

	kernel->bitmap =kernel->memmap->entries[i]->base + kernel->hhdm;

	//fill the bitmap with 1
	for(uint64_t index = 0;index < kernel->bitmap_size;index++){
		kernel->bitmap[index] = (uint64_t)-1;
	}

	//get all usable segment
	//and set it as free
	for (uint64_t i = 0; i < kernel->memmap->entry_count; i++){
		if(kernel->memmap->entries[i]->type != LIMINE_MEMMAP_USABLE)
			continue;
		uint64_t addr = kernel->memmap->entries[i]->base / 0x1000;
		uint64_t end = addr + (kernel->memmap->entries[i]->length / 0x1000);
		while (addr < end){
			free_page(kernel->bitmap,addr);
			addr++;
		}
	}
	

	kprintf("use segment %lu\n",i);
	
	kok();
}