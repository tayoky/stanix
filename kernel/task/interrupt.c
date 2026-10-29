#include <kernel/interrupt.h>
#include <kernel/scheduler.h>
#include <kernel/userspace.h>
#include <kernel/signal.h>
#include <kernel/print.h>
#include <kernel/arch.h>
#include <kernel/vmm.h>

// generic interrupt handler

void timer_handler(registers_t *frame) {
	yield(1);

	if (arch_registers_is_userspace(frame)) {
		return_to_userspace(frame);
	}
}

// called on page faults
int page_fault_handler(registers_t *frame) {
	if (arch_registers_is_userspace(frame) && arch_fault_get_addr(frame) == MAGIC_SIGRETURN) {
		signal_restore_handler(frame);
		return 1;
	}

	// maybee the vmm can handle it
	if (vmm_fault_report(arch_fault_get_addr(frame), arch_fault_get_prot(frame))) {
		// even if the vmm handlded the fault
		// it does not handle return to userspace
		if (arch_registers_is_userspace(frame)) {
			return_to_userspace(frame);
		}
		return 1;
	}
	if (!arch_registers_is_userspace(frame)) {
		// at this point if it's kernel it's probably a panic
		return 0;
	}

	// print some info for debuging
	switch (arch_fault_get_prot(frame)) {
	case MMU_FLAG_READ:
		kdebugf("userspace (%lx) tryied to read %lx\n", PC_REG(*frame), arch_fault_get_addr(frame));
		break;
	case MMU_FLAG_WRITE:
		kdebugf("userspace (%lx) tryied to write %lx\n", PC_REG(*frame), arch_fault_get_addr(frame));
		break;
	case MMU_FLAG_EXEC:
		kdebugf("userspace (%lx) tryied to execute %lx\n", PC_REG(*frame), arch_fault_get_addr(frame));
		break;
	}

	// TODO : remove this
	rwsem_acquire_read(&vmm_get_current_space()->lock);
	for (avltree_node_t *node = avltree_first(&vmm_get_current_space()->segs); node; node = avltree_next(&vmm_get_current_space()->segs, node)) {
		vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
		char prot[5];
		prot[0] = seg->prot & MMU_FLAG_READ ? 'r' : '-';
		prot[1] = seg->prot & MMU_FLAG_WRITE ? 'w' : '-';
		prot[2] = seg->prot & MMU_FLAG_EXEC ? 'x' : '-';
		prot[3] = seg->flags & VMM_FLAG_PRIVATE ? 'p' : 's';
		prot[4] = '\0';

		char *name = NULL;
		if (seg->flags & VMM_FLAG_ANONYMOUS) {
			name = strdup("[anonymous]");
		} else {
			name = vfs_dentry_path(seg->fd->dentry);
		}
		kdebugf("%012lx-%012lx %s %6zd %s\n", seg->start, seg->end, prot, seg->offset, name);
		kfree(name);
	}
	rwsem_release_read(&vmm_get_current_space()->lock);
	arch_registers_stacktrace(frame);

	return 0;
}

int fpu_fault_handler(registers_t *frame) {
	(void)frame;
	if (!(task_get_current_flags() & TASK_FLAG_FPU)) {
		// we need to enable the fpu (lazy fpu init)
		preempt_disable();
		atomic_fetch_or(&get_current_task()->flags, TASK_FLAG_FPU);
		arch_fpu_enable();
		arch_fpu_load(&get_current_task()->context.fpu);
		preempt_enable();
		return 1;
	}
	return 0;
}
