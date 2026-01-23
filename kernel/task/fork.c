#include <kernel/fork.h>
#include <kernel/arch.h>
#include <kernel/scheduler.h>
#include <kernel/memseg.h>
#include <kernel/print.h>
#include <kernel/string.h>
#include <kernel/arch.h>

pid_t fork(void) {
	process_t *parent = get_current_proc();
	process_t *child = new_proc();
	child->parent = parent;

	kdebugf("forking child : %ld\n", child->pid);

	foreach(node, &parent->memseg) {
		memseg_node_t *memseg_node = (memseg_node_t*)node;
		memseg_clone(parent, child, memseg_node->seg);
	}

	//clone metadata
	child->heap_end   = parent->heap_end;
	child->heap_start = parent->heap_start;
	child->main_thread->sig_mask   = get_current_task()->sig_mask;
	memcpy(child->main_thread->sig_handling, get_current_task()->sig_handling, sizeof(get_current_task()->sig_handling));

	//clone fd
	for (int i = 0;i < MAX_FD;i++) {
		child->fds[i] = parent->fds[i];
		if (child->fds[i].present) {
			child->fds[i].fd   = vfs_dup(parent->fds[i].fd);
			child->fds[i].offset = parent->fds[i].offset;
			child->fds[i].flags  = parent->fds[i].flags;
		}
	}

	child->cwd_node = vfs_dup_node(parent->cwd_node);
	child->cwd_path = strdup(parent->cwd_path);

	//copy context parent to child but overload regs with userspace context
	save_context(&child->main_thread->context);
	child->main_thread->context.frame = *get_current_task()->syscall_frame;

	//return 0 to the child
	RET_REG(child->main_thread->context.frame) = 0;

	//make it ruuuunnnnn !!!
	unblock_task(child->main_thread);

	return child->pid;
}
