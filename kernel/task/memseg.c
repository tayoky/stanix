#include <kernel/scheduler.h>
#include <kernel/memseg.h>
#include <kernel/paging.h>
#include <kernel/string.h>
#include <kernel/kernel.h>
#include <kernel/list.h>
#include <kernel/print.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>

memseg *memseg_create(process *proc,uintptr_t address,size_t size,uint64_t prot,int flags){
	list_node *prev = NULL;
	if(address){
		//we need to page align everything
		uintptr_t end = PAGE_ALIGN_UP(address + size);
		address = PAGE_ALIGN_DOWN(address);
		size = end - address;
		foreach(node,proc->memseg){
			memseg *current = node->value;
			if(current->addr < end && current->addr + current->size > address){
				//there already a seg here
				return NULL;
			}
			if(current->addr > end){
				break;
			}
			prev = node;
		}
	} else {
		//no address ? we need to find one ourself
		foreach(node,proc->memseg){
			memseg *current = node->value;
			memseg *next = node->next ? node->next->value : NULL;
			if(!next || next->addr - (current->addr + current->size) >= size){
				address = current->addr + current->size;
				prev = node;
				break;
			}
		}

		if(!address){
			//hardcoded userspace start
			address = PAGE_SIZE * 12;
			prev = NULL;
		}
	}

	memseg *new_memseg = kmalloc(sizeof(memseg));
	memset(new_memseg,0,sizeof(memseg));
	new_memseg->addr = address;
	new_memseg->size = size;
	new_memseg->prot = prot;
	new_memseg->flags = flags;
	new_memseg->ref_count = 1;

	list_add_after(proc->memseg,prev,new_memseg);

	return new_memseg;
}

int memseg_map(process *proc, uintptr_t address,size_t size,uint64_t prot,int flags,vfs_node *node,off_t offset,memseg **seg){
	// we need size to be aligned
	size = PAGE_ALIGN_UP(size);

	memseg *new_memseg = memseg_create(proc,address,size,prot,flags);
	if(!new_memseg) return -EEXIST;
	
	address = new_memseg->addr;
	size    = new_memseg->size;

	//kdebugf("map %p size : %lx\n",address,size);
	int ret = 0;
	if(flags & MAP_ANONYMOUS){
		uintptr_t end = address + size;
		while(address < end){
			map_page(proc->addrspace,pmm_allocate_page(),address,prot);
			address += PAGE_SIZE;
		}
	} else if(node){
		ret = vfs_mmap(node,offset,new_memseg);
	} else {
		ret = -EINVAL;
	}

	if(ret < 0){
		list_remove(proc->memseg,new_memseg);
		kfree(new_memseg);
	}
	
	if(seg)*seg = new_memseg;
	return ret;
}

void memseg_chflag(process *proc,memseg *seg,uint64_t prot){
	seg->prot = prot;
	
	uintptr_t addr = seg->addr;
	uintptr_t end = seg->addr + seg->size;
	while(addr < end){
		map_page(proc->addrspace,(uintptr_t)space_virt2phys(proc->addrspace,(void *)addr),addr,prot);
		addr += PAGE_SIZE;
	}
}

void memseg_unmap(process *proc,memseg *seg){
	list_remove(proc->memseg,seg);

	//kdebugf("unmap %p %p\n",seg->addr,seg->size);


	uintptr_t addr = seg->addr;
	uintptr_t end = seg->addr + seg->size;

	seg->ref_count--;
	if(seg->ref_count == 0){
		if(seg->unmap){
			seg->unmap(seg);
		} else {
			uintptr_t addr = seg->addr;
			uintptr_t end = seg->addr + seg->size;
			while(addr < end){
				pmm_free_page((uintptr_t)space_virt2phys(proc->addrspace,(void *)addr));
				addr += PAGE_SIZE;
			}
		}
		kfree(seg);
	}

	while(addr < end){
		unmap_page(proc->addrspace,addr);
		addr += PAGE_SIZE;
	}
}

void memseg_clone(process *parent,process *child,memseg *seg){
	if(seg->flags & MAP_SHARED){
		seg->ref_count++;
		list_node *prev = NULL;
		foreach(node,child->memseg){
			memseg *current = node->value;
			if(current->addr > seg->addr + seg->size){
				break;
			}
			prev = node;
		}
		list_add_after(child->memseg,prev,seg);

		//now remap in child
		char *addr = (char *)seg->addr;
		char *end = (char *)seg->addr + seg->size;
		while(addr < end){
			uintptr_t phys = (uintptr_t)space_virt2phys(parent->addrspace,addr);
			map_page(child->addrspace,phys,(uintptr_t)addr,seg->prot);
			addr += PAGE_SIZE;
		}
		return;
	}
	//TODO : what happend for device mapped as private ?
	memseg *new_seg;
	if(memseg_map(child,seg->addr,seg->size,PAGING_FLAG_RW_CPL0,seg->flags | MAP_ANONYMOUS,NULL,0,&new_seg) < 0){
		return;
	}

	//copy content
	char *addr = (char *)seg->addr;
	char *end = (char *)seg->addr + seg->size;
	while(addr < end){
		uintptr_t phys = (uintptr_t)space_virt2phys(child->addrspace,addr);
		memcpy((void *)kernel->hhdm + phys,addr,PAGE_SIZE);
		addr += PAGE_SIZE;
	}

	memseg_chflag(child,new_seg,seg->prot);
}
