#include "bitmap.h"
#include "kernel.h"
#include "print.h"
#include "limine.h"

void init_bitmap(kernel_table *kernel){
	kstatus("init memory bitmap ...");

	//create an bitmap of the correspondig size
	//each unint64_t can store state for 64 page of 4KB each so 65536 bytes
	kernel->bitmap_size = kernel->total_memory/65536+1;

	//find a good segment we can use
	uint64_t i=0;
	while (kernel->bootinfo.memmap_response->entries[i]->type != LIMINE_MEMMAP_USABLE  || kernel->bootinfo.memmap_response->entries[i]->length < kernel->bitmap_size * sizeof(uint64_t) ){
		i++;
		if(i >= kernel->bootinfo.memmap_response->entry_count){
			kfail();
			kstatus("this is a vital step in init the kernel\n");
			kstatus("can't boot\n");
			halt();
		}
	}
	kernel->bitmap = kernel->bootinfo.memmap_response->entries[i]->base + kernel->bootinfo.hhdm_response->offset;

	//fill the bitmap with 0
	for(uint64_t index = 0;index < kernel->bitmap_size;index++){
		kernel->bitmap[index] = 0;
	}

	kprintf("use segment %lu\n",i);
	
	kok();
}