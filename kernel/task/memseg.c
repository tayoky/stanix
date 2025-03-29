#include "memseg.h"
#include <stdint.h>
#include "paging.h"
#include "scheduler.h"
#include "string.h"

memseg *memseg_map(process *proc, uint64_t address,size_t size,uint64_t flags){
	//first convert all of that into pages
	address = PAGE_ALIGN_DOWN(address) / PAGE_SIZE;
	size = PAGE_ALIGN_UP(size) / PAGE_SIZE;

	memseg *new_memseg = kmalloc(sizeof(memseg));
	if(proc->first_memseg){
		proc->first_memseg->prev = new_memseg;
	}
	new_memseg->next = proc->first_memseg;
	new_memseg->prev = NULL;
	proc->first_memseg = new_memseg;

	new_memseg->offset = (void*)(address * PAGE_SIZE);
	new_memseg->size = size;
	new_memseg->flags = flags;


	while(size > 0){
		map_page((void *)proc->cr3 + kernel->hhdm,allocate_page(&kernel->bitmap),address,flags);
		address++;
		size--;
	}

	return new_memseg;
}

void memeseg_chflag(process *proc,memseg *seg,uint64_t flags){
	
	size_t size = seg->size;
	uint64_t address = (uintptr_t)seg->offset / PAGE_SIZE;
	uint64_t *PMLT4 = (uint64_t *)(proc->cr3 + kernel->hhdm);
	seg->flags = flags;
	while(size > 0){
		map_page(PMLT4,(uintptr_t)space_virt2phys(PMLT4,(void *)(address * PAGE_SIZE))/PAGE_SIZE,address,flags);
		address++;
		size--;
	}
}

void memseg_unmap(process *proc,memseg *seg){
	//link the two sides
	if(seg->prev){
		seg->prev->next = seg->next;
	}
	
	if(seg->next){
		seg->next->prev = seg->prev;
	}

	if(proc->first_memseg == seg){
		proc->first_memseg = NULL;
	}

	//unmap
	size_t size = seg->size;
	uint64_t virt_page = (uint64_t)seg->offset / PAGE_SIZE;
	while(size > 0){
		//get the phys page
		uint64_t phys_page = (uint64_t)space_virt2phys((void *)proc->cr3 + kernel->hhdm,(void *)(virt_page * PAGE_SIZE)) / PAGE_SIZE;

		//unmap it
		unmap_page((void *)proc->cr3 + kernel->hhdm,virt_page);

		//free it
		free_page(&kernel->bitmap,phys_page);
		
		virt_page++;
		size--;
	}

	kfree(seg);
}

void memseg_clone(process *parent,process *child,memseg *seg){
	(void)parent;

	memseg *new_seg = memseg_map(child,(uintptr_t)seg->offset,seg->size * PAGE_SIZE,seg->flags);
	
	size_t size = new_seg->size;
	uint64_t virt_addr = (uint64_t)new_seg->offset;
	while(size > 0){
		//get the phys page
		void * phys_addr = space_virt2phys((void *)child->cr3 + kernel->hhdm,(void *)virt_addr);

		memcpy((void *)phys_addr + kernel->hhdm, (void *)virt_addr ,PAGE_SIZE);
		
		virt_addr+= PAGE_SIZE;
		size--;
	}

}
