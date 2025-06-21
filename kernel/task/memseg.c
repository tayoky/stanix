#include <kernel/scheduler.h>
#include <kernel/memseg.h>
#include <kernel/paging.h>
#include <kernel/string.h>
#include <kernel/kernel.h>
#include <stdint.h>
#include <stddef.h>
#include <kernel/print.h>

memseg *memseg_map(process *proc, uintptr_t address,size_t size,uint64_t flags){
	memseg *prev = NULL;
	if(address){
		//we need to page align everything
		uintptr_t end = PAGE_ALIGN_UP(address + size);
		address = PAGE_ALIGN_DOWN(address);
		size = end - address;
		memseg *current = proc->first_memseg;
		while(current){
			if(current->addr < end && current->addr + current->size > address){
				//we are trying to map an already mapped region
				return NULL;
			}
			if(!current->next || current->next->addr > end){
				prev = current;
				break;
			}
			current = current->next;
		}
	} else {
		//no address ? we need to find one ourself
		memseg *current = proc->first_memseg;
		while(current){
			if(!current->next || current->next->addr - current->addr - current->size > size){
				address = current->addr + current->size;
				prev = current;
				break;
			}
			current = current->next;
		}

		if(!address){
			//hardcoded userspace start
			address = PAGE_SIZE * 12;
			prev = NULL;
		}
	}

	kdebugf("map %p size : %lx\n",address,size);
	memseg *new_memseg = kmalloc(sizeof(memseg));
	memset(new_memseg,0,sizeof(memseg));
	new_memseg->addr = address;
	new_memseg->size = size;
	new_memseg->flags = flags;

	//now link the node
	new_memseg->prev = prev;
	if(prev){
		new_memseg->next = prev->next;
		prev->next = new_memseg;
	} else {
		new_memseg->next = proc->first_memseg;
		proc->first_memseg = new_memseg;
	}
	if(new_memseg->next){
		new_memseg->next->prev = new_memseg;
	}

	uintptr_t end = address + size;
	while(address < end){
		map_page(proc->addrspace,pmm_allocate_page(),address,flags);
		address += PAGE_SIZE;
	}

	return new_memseg;
}

void memeseg_chflag(process *proc,memseg *seg,uint64_t flags){
	seg->flags = flags;
	
	uintptr_t addr = seg->addr;
	uintptr_t end = seg->addr + seg->size;
	while(addr < end){
		map_page(proc->addrspace,pmm_allocate_page(),addr,flags);
		addr += PAGE_SIZE;
	}
}

void memseg_unmap(process *proc,memseg *seg){
	
}

void memseg_clone(process *parent,process *child,memseg *seg){
	
}
