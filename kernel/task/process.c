#include <kernel/process.h>
#include <kernel/slab.h>
#include <kernel/xarray.h>

static slab_cache_t process_group_slabs;
static xarray_t groups;
xarray_t procs;

void init_proc(void) {
	slab_init(&process_groups_slab, sizeof(process_group_t), "process-groups");
}

process_group_t *process_group_get_from_pgid(pid_t pgid) {
	rcu_acquire_read(&groups.rcu);
	process_group_t *group = xarray_get(&groups, pgid);
	process_group_ref(group);
	rcu_release_read(&groups.rcu);
	return group;
}

process_group_t *process_group_get_or_create_from_pgid(pid_t pgid) {
	process_group_t *group = process_group_get_from_pgid(pgid);
	if (group) return group;

	// we need to create a group
	group = slab_alloc(&process_groups_slab);
	if (!group) return NULL;
	memset(group, 0, sizeof(process_group_t));
	group->ref_count = 1;

	rcu_acquire_read(&groups.rcu);
	process_group_t *race_group = xarray_cmpxchg(&groups, pgid, NULL, group);
	process_group_ref(race_group);
	rcu_release_read(&groups.rcu);
	if (race_group) {
		// we raced
		slab_free(group);
		return race_group;
	}
	return group;
}

void process_group_release(process_group_t *group) {
	if (!group) return;
	if (ref_count_dec(&group->ref) > 1) return;
	slab_free(group);
}

void proc_set_group(process_t *proc, process_group_t *group) {
	spinlock_assert_acquired(&proc->proc_lock);
	spinlock_assert_acquired(&proctree_lock);
	if (proc->group) {
		rculist_remove(&proc->group->processes, &proc->group_node);
		if (rculist_is_empty(&proc->group->processes)) {
			xarray_clear(&groups, &proc->group->pgid);
		}
		process_group_release(proc->group);
	}
	proc->group = process_group_ref(group);
	if (group) {
		rculist_append(&group->processes, &proc->group_node);
	}
}

process_t *proc_from_pid(pid_t pid) {
	// is it ourself ?
	if (get_current_proc()->pid == pid) {
		return proc_ref(get_current_proc());
	}

	rcu_acquire_read(&procs.rcu);
	process_t *proc = xarray_get(&procs, pid);
	proc_ref(proc);
	rcu_release_read(&procs.rcu);
	return proc;
}

static void proc_register(process_t *proc) {
	xarray_set(&procs, proc->pid, proc);
}

static void proc_unregister(process_t *proc) {
	xarray_set(&procs, proc->pid, NULL);
}

void proc_release(process_t *proc) {
	if (!proc) return;
	if (ref_count_dec(&proc->ref_count) > 1) {
		return;
	}

	// now we can free the address space
	vmm_destroy_space(&proc->vmm_space);

	kfree(proc->cmdline);
	kfree(proc);
}

process_t *new_proc(void (*func)(void *arg), void *arg) {
	// init the new proc
	process_t *proc = kmalloc(sizeof(process_t));
	memset(proc, 0, sizeof(process_t));

	spinlock_acquire(&proc->proc_lock);
	spinlock_acquire(&proctree_lock);

	proc->parent = get_current_proc();
	proc->state  = PROC_STATE_RUNNING;
	vmm_init_space(&proc->vmm_space);
	list_init(&proc->child);
	list_init(&proc->threads);
	proc_set_group(proc, get_current_proc()->group);
	rcu_acquire_read(NULL);
	proc_set_cred(proc, get_current_cred());
	rcu_release_read(NULL);
	proc->umask       = get_current_proc()->umask;
	proc->cmdline     = strdup(get_current_proc()->cmdline);
	proc->cwd         = vfs_dentry_ref(get_current_proc()->cwd);
	proc->exe         = vfs_dentry_ref(get_current_proc()->exe);
	proc->main_thread = new_task(proc, func, arg);
	proc->pid         = proc->main_thread->tid;

	// add it the the list of the children of the parent
	// note that the parent hold a ref
	proc_ref(proc);
	list_append(&proc->parent->child, &proc->child_list_node);

	// add it to the global process list
	// note that the proc list only hold a weak ref
	proc_register(proc);
	
	spinlock_release(&proctree_lock);
	spinlock_release(&proc->proc_lock);

	return proc;
}

void kill_proc(void) {
	// just kill the main thread
	if (get_current_task() == get_current_proc()->main_thread) {
		// we are the main thread, diying will kill the proc
		kill_task();
	} else {
		// we are not the main thread
		signal_send_task(get_current_proc()->main_thread, SIGKILL);
		kill_task();
	}
}

static void alert_parent(process_t *proc) {
	if (!proc->parent) return;
	signal_send(proc->parent, SIGCHLD);
}

void do_proc_deletion(void) {
	// all the childreen become orphelan
	// the parent of orphelan is init
	spinlock_acquire(&get_current_proc()->proc_lock);
	spinlock_acquire(&proctree_lock);
	list_node_t *node = get_current_proc()->child.first_node;
	while (node) {
		process_t *child = container_of(node, process_t, child_list_node);
		node = node->next;

		child->parent = init;
		list_append(&init->child, &child->child_list_node);
		if (proc_get_state(child) == TASK_STATUS_ZOMBIE) alert_parent(child);
	}
	list_destroy(&get_current_proc()->child);

	// release session / group / cred
	proc_set_group(get_current_proc(), NULL);
	cred_release(get_current_cred());

	proc_set_state(get_current_proc(), PROC_STATE_ZOMBIE);

	spinlock_release_acquire(&proctree_lock);
	spinlock_release(&get_current_proc()->proc_lock);

	// close every open fd
	for (size_t i = 0; i < MAX_FD; i++) {
		if (get_current_proc()->fd_table.fds[i].present) {
			close_fd(i);
		}
	}

	// release locked dentry
	vfs_dentry_release(get_current_proc()->cwd);
	vfs_dentry_release(get_current_proc()->exe);

	vmm_unmap_all();

	list_destroy(&get_current_proc()->threads);
}

void proc_zombie_cleanup(process_t *proc) {
	spinlock_assert_acquired(&proctree_lock);
	proc_unregister(proc);
	if (proc->parent) list_remove(&proc->parent->child, &proc->child_list_node);

	// the parent hold a ref that we need to free
	proc_release(proc);
}
