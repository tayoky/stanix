#include <kernel/scheduler.h>
#include <kernel/vmm.h>
#include <kernel/mmu.h>
#include <kernel/string.h>
#include <kernel/kernel.h>
#include <kernel/list.h>
#include <kernel/print.h>
#include <kernel/pmm.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>

static int vmm_handle_fault(vmm_seg_t *seg, uintptr_t addr, int prot) {
	// TODO : CoW and stuff
	(void)seg;
	(void)addr;
	(void)prot;
	return 0;
}

int vmm_fault_report(uintptr_t addr, int prot) {
	foreach (node, &get_current_proc()->vmm_seg) {
		vmm_seg_t *seg = (vmm_seg_t*)node;
		if (seg->start > addr) break;
		if (seg->end > addr) {
			// we found a seg to report too
			return vmm_handle_fault(seg, addr, prot);
		} 
	}
	return 0;
}

vmm_seg_t *vmm_create_seg(process_t *proc, uintptr_t address, size_t size, long prot, int flags) {
	vmm_seg_t *prev = NULL;
	if (address) {
		// we need to page align everything
		uintptr_t end = PAGE_ALIGN_UP(address + size);
		address = PAGE_ALIGN_DOWN(address);
		foreach(node, &proc->vmm_seg) {
			vmm_seg_t *current = (vmm_seg_t*)node;
			if (current->start < end && current->end > address) {
				// there is aready a seg here
				return NULL;
			}
			if (current->start > end) {
				break;
			}
			prev = current;
		}
	} else {
		// we need to page align everything
		size = PAGE_ALIGN_UP(size);

		// no address ? we need to find one ourself
		foreach(node, &proc->vmm_seg) {
			vmm_seg_t *current = (vmm_seg_t*)node;
			vmm_seg_t *next = (vmm_seg_t*)node->next;
			if (!next || next->start - current->end >= size) {
				address = current->end;
				prev = current;
				break;
			}
		}

		if (!address) {
			// hardcoded userspace start
			address = PAGE_SIZE * 12;
			prev = NULL;
		}
	}

	vmm_seg_t *new_seg = kmalloc(sizeof(vmm_seg_t));
	memset(new_seg, 0, sizeof(vmm_seg_t));
	new_seg->start = address;
	new_seg->end   = address + size;
	new_seg->prot  = prot;
	new_seg->flags = flags;

	list_add_after(&proc->vmm_seg, prev ? &prev->node : NULL, &new_seg->node);

	return new_seg;
}

int vmm_map(process_t *proc, uintptr_t address, size_t size, long prot, int flags, vfs_fd_t *fd, off_t offset, vmm_seg_t **seg) {
	// we need size to be aligned
	size = PAGE_ALIGN_UP(size);

	vmm_seg_t *new_seg = vmm_create_seg(proc, address, size, prot, flags);
	if (!new_seg) return -EEXIST;

	//kdebugf("map %p size : %lx\n",address,size);
	int ret = 0;
	if (flags & VMM_FLAG_ANONYMOUS) {
		fd = NULL;
		for (uintptr_t addr=new_seg->start; addr < new_seg->end; addr += PAGE_SIZE) {
			// TODO : handle error from pmm_allocate_page
			mmu_map_page(proc->addrspace, pmm_allocate_page(), addr, prot);
		}
	} else if (fd) {
		ret = vfs_mmap(fd, offset, new_seg);
	} else {
		ret = -EINVAL;
	}

	if (ret < 0) {
		list_remove(&proc->vmm_seg, &new_seg->node);
		kfree(new_seg);
	} else {
		if (seg) *seg = new_seg;
		if (fd) new_seg->fd = vfs_dup(fd);
	}
	return ret;
}

int vmm_chprot(process_t *proc, vmm_seg_t *seg, long prot) {
	if (seg->ops && seg->ops->can_mprotect) {
		int ret = seg->ops->can_mprotect(seg, prot);
		if (ret < 0) return ret;
	}
	seg->prot = prot;

	for (uintptr_t addr=seg->start; addr < seg->end; addr += PAGE_SIZE) {
		mmu_map_page(proc->addrspace, mmu_space_virt2phys(proc->addrspace, (void *)addr), addr, prot);
	}
	return 0;
}

void vmm_unmap(process_t *proc, vmm_seg_t *seg) {
	list_remove(&proc->vmm_seg, &seg->node);

	//kdebugf("unmap %p %p\n",seg->addr,seg->size);

	if (seg->ops && seg->ops->close) {
		seg->ops->close(seg);
	}
	vfs_close(seg->fd);

	// IO cannot be allocated/freed using the PMM
	// so do not free them
	// it's the driver job to do it
	if (!(seg->flags & VMM_FLAG_IO)) {
		for (uintptr_t addr=seg->start; addr < seg->end; addr += PAGE_SIZE) {
			pmm_free_page(mmu_space_virt2phys(proc->addrspace, (void *)addr));
		}
	}

	for (uintptr_t addr=seg->start; addr < seg->end; addr += PAGE_SIZE) {
		mmu_unmap_page(proc->addrspace, addr);
	}

	kfree(seg);
}

void vmm_clone(process_t *parent, process_t *child, vmm_seg_t *seg) {
	vmm_seg_t *new_seg = kmalloc(sizeof(vmm_seg_t));
	memset(new_seg, 0, sizeof(vmm_seg_t));
	new_seg->prot         = seg->prot;
	new_seg->flags        = seg->flags;
	new_seg->start        = seg->start;
	new_seg->end          = seg->end;
	new_seg->ops          = seg->ops;
	new_seg->private_data = seg->private_data;
	new_seg->fd           = vfs_dup(seg->fd);

	if (seg->flags & VMM_FLAG_SHARED) {
		vmm_seg_t *prev = NULL;
		foreach(node, &child->vmm_seg) {
			vmm_seg_t *current = (vmm_seg_t*)node;
			if (current->start > seg->end) {
				break;
			}
			prev = current;
		}
		list_add_after(&child->vmm_seg, prev ? &prev->node : NULL, &new_seg->node);

		// now remap in child
		for (uintptr_t addr=seg->start; addr < seg->end; addr += PAGE_SIZE) {
			uintptr_t phys = mmu_space_virt2phys(parent->addrspace, (void*)addr);
			// do not touch ref count of IO mapping
			// because they actually do not have a ref count
			if (!(seg->flags & VMM_FLAG_IO)) {
				pmm_retain(phys);
			}
			mmu_map_page(child->addrspace, phys, addr, seg->prot);
		}
		return;
	}

	// TODO : do CoW here

	// TODO : what happend for device mapped as private ?
	if (vmm_map(child, seg->start, VMM_SIZE(seg), MMU_FLAG_WRITE | MMU_FLAG_PRESENT, seg->flags | VMM_FLAG_ANONYMOUS, NULL, 0, &new_seg) < 0) {
		return;
	}

	// copy content
	for (uintptr_t addr=seg->start; addr < seg->end; addr += PAGE_SIZE) {
		uintptr_t phys = mmu_space_virt2phys(child->addrspace, (void*)addr);
		memcpy((void *)(kernel->hhdm + phys), (void*)addr, PAGE_SIZE);
	}

	vmm_chprot(child, new_seg, seg->prot);
}
