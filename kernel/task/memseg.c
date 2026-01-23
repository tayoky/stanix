#include <kernel/scheduler.h>
#include <kernel/memseg.h>
#include <kernel/mmu.h>
#include <kernel/string.h>
#include <kernel/kernel.h>
#include <kernel/list.h>
#include <kernel/print.h>
#include <kernel/pmm.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>

static int memseg_handle_fault(memseg_t *seg, uintptr_t addr, int prot) {
	// TODO : CoW and stuff
	(void)seg;
	(void)addr;
	(void)prot;
	return 0;
}

int memseg_fault_report(uintptr_t addr, int prot) {
	foreach (node, &get_current_proc()->memseg) {
		memseg_node_t *memseg_node = (memseg_node_t*)node;
		memseg_t *seg = memseg_node->seg;
		if (seg->addr > addr) break;
		if (seg->addr + seg->size > addr) {
			// we found a seg to report too
			return memseg_handle_fault(seg, addr, prot);
		} 
	}
	return 0;
}

memseg_t *memseg_create(process_t *proc, uintptr_t address, size_t size, uint64_t prot, int flags) {
	memseg_node_t *prev = NULL;
	if (address) {
		//we need to page align everything
		uintptr_t end = PAGE_ALIGN_UP(address + size);
		address = PAGE_ALIGN_DOWN(address);
		size = end - address;
		foreach(node, &proc->memseg) {
			memseg_node_t *current = (memseg_node_t*)node;
			if (current->seg->addr < end && current->seg->addr + current->seg->size > address) {
				//there already a seg here
				return NULL;
			}
			if (current->seg->addr > end) {
				break;
			}
			prev = current;
		}
	} else {
		//no address ? we need to find one ourself
		foreach(node, &proc->memseg) {
			memseg_node_t *current = (memseg_node_t*)node;
			memseg_node_t *next = (memseg_node_t*)node->next;
			if (!next || next->seg->addr - (current->seg->addr + current->seg->size) >= size) {
				address = current->seg->addr + current->seg->size;
				prev = current;
				break;
			}
		}

		if (!address) {
			//hardcoded userspace start
			address = PAGE_SIZE * 12;
			prev = NULL;
		}
	}

	memseg_t *new_memseg = kmalloc(sizeof(memseg_t));
	memset(new_memseg, 0, sizeof(memseg_t));
	new_memseg->addr = address;
	new_memseg->size = size;
	new_memseg->prot = prot;
	new_memseg->flags = flags;
	new_memseg->ref_count = 1;

	memseg_node_t *memseg_node = kmalloc(sizeof(memseg_node_t));
	memset(memseg_node, 0, sizeof(memseg_node_t));
	memseg_node->seg = new_memseg;

	list_add_after(&proc->memseg, prev ? &prev->node : NULL, &memseg_node->node);

	return new_memseg;
}

static void remove_seg(process_t *proc, memseg_t *seg) {
	foreach(node, &proc->memseg) {
		memseg_node_t *memseg_node = (memseg_node_t*)node;
		if (memseg_node->seg == seg) {
			list_remove(&proc->memseg, node);
			return;
		}
	}
}

int memseg_map(process_t *proc, uintptr_t address, size_t size, uint64_t prot, int flags, vfs_fd_t *fd, off_t offset, memseg_t **seg) {
	// we need size to be aligned
	size = PAGE_ALIGN_UP(size);

	memseg_t *new_memseg = memseg_create(proc, address, size, prot, flags);
	if (!new_memseg) return -EEXIST;

	address = new_memseg->addr;
	size    = new_memseg->size;

	//kdebugf("map %p size : %lx\n",address,size);
	int ret = 0;
	if (flags & MAP_ANONYMOUS) {
		fd = NULL;
		uintptr_t end = address + size;
		while (address < end) {
			mmu_map_page(proc->addrspace, pmm_allocate_page(), address, prot);
			address += PAGE_SIZE;
		}
	} else if (fd) {
		ret = vfs_mmap(fd, offset, new_memseg);
	} else {
		ret = -EINVAL;
	}

	if (ret < 0) {
		remove_seg(proc, new_memseg);
		kfree(new_memseg);
	} else {
		if (seg) *seg = new_memseg;
		if (fd) new_memseg->fd = vfs_dup(fd);
	}
	return ret;
}

void memseg_chflag(process_t *proc, memseg_t *seg, uint64_t prot) {
	seg->prot = prot;

	uintptr_t addr = seg->addr;
	uintptr_t end = seg->addr + seg->size;
	while (addr < end) {
		mmu_map_page(proc->addrspace, mmu_space_virt2phys(proc->addrspace, (void *)addr), addr, prot);
		addr += PAGE_SIZE;
	}
}

void memseg_unmap(process_t *proc, memseg_t *seg) {
	remove_seg(proc, seg);

	//kdebugf("unmap %p %p\n",seg->addr,seg->size);

	uintptr_t addr = seg->addr;
	uintptr_t end = seg->addr + seg->size;

	seg->ref_count--;
	if (seg->ref_count == 0) {
		if (seg->fd) {
			vfs_munmap(seg->fd, seg);
			vfs_close(seg->fd);
		} else {
			uintptr_t addr = seg->addr;
			uintptr_t end = seg->addr + seg->size;
			while (addr < end) {
				pmm_free_page(mmu_space_virt2phys(proc->addrspace, (void *)addr));
				addr += PAGE_SIZE;
			}
		}
		kfree(seg);
	}

	while (addr < end) {
		mmu_unmap_page(proc->addrspace, addr);
		addr += PAGE_SIZE;
	}
}

void memseg_clone(process_t *parent, process_t *child, memseg_t *seg) {
	if (seg->flags & MAP_SHARED) {
		seg->ref_count++;
		memseg_node_t *prev = NULL;
		foreach(node, &child->memseg) {
			memseg_node_t *current = (memseg_node_t*)node;
			if (current->seg->addr > seg->addr + seg->size) {
				break;
			}
			prev = current;
		}
		memseg_node_t *memseg_node = kmalloc(sizeof(memseg_node_t));
		memseg_node->seg = seg;
		list_add_after(&child->memseg, &prev->node, &memseg_node->node);

		// now remap in child
		char *addr = (char *)seg->addr;
		char *end = (char *)seg->addr + seg->size;
		while (addr < end) {
			uintptr_t phys = mmu_space_virt2phys(parent->addrspace, addr);
			mmu_map_page(child->addrspace, phys, (uintptr_t)addr, seg->prot);
			addr += PAGE_SIZE;
		}
		return;
	}
	// TODO : what happend for device mapped as private ?
	memseg_t *new_seg;
	if (memseg_map(child, seg->addr, seg->size, PAGING_FLAG_RW_CPL0, seg->flags | MAP_ANONYMOUS, NULL, 0, &new_seg) < 0) {
		return;
	}

	//copy content
	char *addr = (char *)seg->addr;
	char *end = (char *)seg->addr + seg->size;
	while (addr < end) {
		uintptr_t phys = mmu_space_virt2phys(child->addrspace, addr);
		memcpy((void *)kernel->hhdm + phys, addr, PAGE_SIZE);
		addr += PAGE_SIZE;
	}

	memseg_chflag(child, new_seg, seg->prot);
}
