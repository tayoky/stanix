#include <kernel/memseg.h>
#include <kernel/paging.h>
#include <kernel/scheduler.h>
#include <kernel/string.h>
#include <kernel/kernel.h>
#include <stdint.h>

memseg *memseg_map(process *proc, uintptr_t address,size_t size,uint64_t flags){
	//first convert all of that into pages
	uintptr_t end = PAGE_ALIGN_UP(address + size);
	address = PAGE_ALIGN_DOWN(address);
	size = (PAGE_ALIGN_UP(end - address) + 1) / PAGE_SIZE;

	memseg *new_memseg = kmalloc(sizeof(memseg));
	if(proc->first_memseg){
		proc->first_memseg->prev = new_memseg;
	}
	new_memseg->next = proc->first_memseg;
	new_memseg->prev = NULL;
	proc->first_memseg = new_memseg;

	new_memseg->offset = (void*)(address);
	new_memseg->size = size;
	new_memseg->flags = flags;


	while(size > 0){
		map_page(proc->addrspace,pmm_allocate_page(),address,flags);
		address += PAGE_SIZE;
		size--;
	}

	return new_memseg;
}

void memeseg_chflag(process *proc,memseg *seg,uint64_t flags){
	
	size_t size = seg->size;
	uintptr_t address = (uintptr_t)seg->offset;
	seg->flags = flags;
	while(size > 0){
		map_page(proc->addrspace,(uintptr_t)space_virt2phys(proc->addrspace,(void *)address),address,flags);
		address += PAGE_SIZE;
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
	uintptr_t virt_page = (uintptr_t)seg->offset;
	while(size > 0){
		//get the phys page
		uintptr_t phys_page = (uintptr_t)space_virt2phys(proc->addrspace,(void *)virt_page);

		//unmap it
		unmap_page(proc->addrspace,virt_page);

		//free it
		pmm_free_page(phys_page);
		
		virt_page += PAGE_SIZE;
		size--;
	}

	kfree(seg);
}

void memseg_clone(process *parent,process *child,memseg *seg){
	(void)parent;

	memseg *new_seg = memseg_map(child,(uintptr_t)seg->offset,seg->size * PAGE_SIZE,seg->flags);
	
	size_t size = new_seg->size;
	uintptr_t virt_addr = (uintptr_t)new_seg->offset;
	while(size > 0){
		//get the phys page
		void * phys_addr = space_virt2phys(child->addrspace,(void *)virt_addr);

		memcpy((void *)((uintptr_t)phys_addr + kernel->hhdm), (void *)virt_addr ,PAGE_SIZE);
		
		virt_addr += PAGE_SIZE;
		size--;
	}

}
