#include <kernel/fork.h>
#include <kernel/arch.h>
#include <kernel/scheduler.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/arch.h>
#include <kernel/vmm.h>

static void fork_trampoline(void *arg) {
	acontext_t context = *(acontext_t*)arg;
	kfree(arg);
	arch_load_context(&context);
}

pid_t fork(void) {
	// setup new context for child
	// and return 0 to the child
	acontext_t *new_context = kmalloc(sizeof(acontext_t));
	arch_save_context(new_context);
	new_context->frame = *get_current_task()->syscall_frame;
	RET_REG(new_context->frame) = 0;

	process_t *parent = get_current_proc();
	process_t *child = new_proc(fork_trampoline, new_context);
	child->parent = parent;

	kdebugf("forking child : %ld\n", child->pid);

	vmm_clone(&parent->vmm_space, &child->vmm_space);

	// clone metadata
	child->heap_end   = parent->heap_end;
	child->heap_start = parent->heap_start;
	child->main_thread->sig_mask   = get_current_task()->sig_mask;
	memcpy(child->main_thread->sig_handling, get_current_task()->sig_handling, sizeof(get_current_task()->sig_handling));

	// clone fd table
	for (int i = 0;i < MAX_FD;i++) {
		if (parent->fd_table.fds[i].present) {
			child->fd_table.fds[i].fd      = vfs_dup(parent->fd_table.fds[i].fd);
			child->fd_table.fds[i].flags   = parent->fd_table.fds[i].flags;
			child->fd_table.fds[i].present = 1;
		}
	}

	// make it ruuuunnnnn !!!
	unblock_task(child->main_thread);

	return child->pid;
}
