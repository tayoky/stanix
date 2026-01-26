#include <kernel/fork.h>
#include <kernel/arch.h>
#include <kernel/scheduler.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/arch.h>
#include <kernel/vmm.h>

static void fork_trampoline(void *arg) {
	kdebugf("reaching fork trampoline\n");
	acontext_t context = *(acontext_t*)arg;
	kfree(arg);

	// since we reconsituied the new context we can now load it
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

	foreach (node, &parent->vmm_seg) {
		vmm_seg_t *seg = (vmm_seg_t *)node;
		vmm_clone(parent, child, seg);
	}

	// clone metadata
	child->heap_end   = parent->heap_end;
	child->heap_start = parent->heap_start;
	child->main_thread->sig_mask   = get_current_task()->sig_mask;
	memcpy(child->main_thread->sig_handling, get_current_task()->sig_handling, sizeof(get_current_task()->sig_handling));

	// clone fd
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

	// make it ruuuunnnnn !!!
	unblock_task(child->main_thread);

	return child->pid;
}
