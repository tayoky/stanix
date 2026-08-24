#include <kernel/process.h>
#include <kernel/slab.h>
#include <kernel/xarray.h>
#include <abi/wait.h>

static slab_cache_t sessions_slab;
static slab_cache_t process_groups_slab;
static slab_cache_t procs_slab;
static xarray_t groups;
xarray_t procs;

void init_proc(void) {
	slab_init(&sessions_slab, sizeof(session_t), "sessions");
	slab_init(&process_groups_slab, sizeof(process_group_t), "process-groups");
	slab_init(&procs_slab, sizeof(process_t), "processes");
	xarray_init(&procs);
	xarray_init(&groups);
}

void session_release(session_t *session) {
	if (!session) return;
	if (ref_count_dec(&session->ref_count) > 1) {
		return;
	}
	slab_free(session);
}

int session_create(process_t *leader) {
	session_t *session = slab_alloc(&sessions_slab);
	if (!session) return -ENOMEM;
	memset(session, 0, sizeof(session_t));
	session->leader = leader;
	session->sid    = leader->pid;
	process_group_t *group = process_group_create(leader->pid);
	if (!group) {
		slab_free(session);
		return -ENOMEM;
	}
	process_group_set_session(group, session);
	
	spinlock_acquire(&leader->proc_lock);
	spinlock_acquire(&proctree_lock);

	int ret = proc_set_group(leader, group);

	spinlock_release(&proctree_lock);
	spinlock_release(&leader->proc_lock);
	process_group_release(group);
	return ret;
}

process_group_t *process_group_from_pgid(pid_t pgid) {
	rcu_acquire_read(&groups.rcu);
	process_group_t *group = xarray_get(&groups, pgid);
	process_group_ref(group);
	rcu_release_read(&groups.rcu);
	return group;
}

process_group_t *process_group_create(pid_t pgid) {
	// we need to create a group
	process_group_t *group = slab_alloc(&process_groups_slab);
	if (!group) return NULL;
	memset(group, 0, sizeof(process_group_t));
	group->ref_count = 1;
	group->pgid = pgid;
	xarray_set(&groups, group->pgid, group);

	return group;
}

void process_group_set_session(process_group_t *group, session_t *session) {
	kassert(!group->session);
	group->session = session_ref(session);
	rculist_append(&session->groups, &group->node);
}

void process_group_release(process_group_t *group) {
	if (!group) return;
	if (ref_count_dec(&group->ref_count) > 1) return;
	rculist_remove(&group->session->groups, &group->node);
	session_release(group->session);
	xarray_clear(&groups, group->pgid);
	slab_free(group);
}

int proc_set_group(process_t *proc, process_group_t *group) {
	spinlock_assert_acquired(&proc->proc_lock);
	spinlock_assert_acquired(&proctree_lock);
	if (proc->group && proc->group->pgid == proc->pid) {
		// already a process group leader
		return -EPERM;
	}
	if (group == proc->group) return 0;
	if (proc->group) {
		rculist_remove(&proc->group->processes, &proc->group_node);
		if (rculist_is_empty(&proc->group->processes)) {
			xarray_clear(&groups, proc->group->pgid);
		}
		process_group_release(proc->group);
	}
	proc->group = process_group_ref(group);
	if (group) {
		rculist_append(&group->processes, &proc->group_node);
	}
	return 0;
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

	signal_context_destroy(&proc->signal_context);
	kfree(proc->cmdline);
	slab_free(proc);
}

process_t *proc_allocate(void) {
	process_t *proc = slab_alloc(&procs_slab);
	if (!proc) return NULL;
	memset(proc, 0, sizeof(process_t));
	return proc;
}

process_t *proc_new(void (*func)(void *arg), void *arg) {
	// init the new proc
	process_t *proc = proc_allocate();
	if (!proc) return NULL;

	proc->main_thread = task_new(proc, func, arg);
	if (!proc->main_thread) {
		slab_free(proc);
		return NULL;
	}
	
	vmm_init_space(&proc->vmm_space);
	list_init(&proc->child);
	proc->pid    = proc->main_thread->tid;
	proc->state  = PROC_STATE_RUNNING;

	if (!get_current_proc()) {
		session_create(proc);
	}

	spinlock_acquire(&proc->proc_lock);
	spinlock_acquire(&proctree_lock);

	if (get_current_proc()) {
		proc->parent = get_current_proc();
		proc_set_group(proc, get_current_proc()->group);
		rcu_acquire_read(NULL);
		proc_set_cred(proc, get_current_cred());
		rcu_release_read(NULL);
		proc->umask       = get_current_proc()->umask;
		proc->cmdline     = strdup(get_current_proc()->cmdline);
		proc->cwd         = vfs_dentry_ref(get_current_proc()->cwd);
		proc->exe         = vfs_dentry_ref(get_current_proc()->exe);

		// add it the the list of the children of the parent
		// note that the parent hold a ref
		proc_ref(proc);
		list_append(&proc->parent->child, &proc->child_list_node);
	} else {
		// not current process running
		// init with sane values
		proc_set_cred(proc, &default_cred);
		proc->umask  = 022;
		proc->cmdline = strdup("unknown");
		proc->cwd = vfs_get_dentry("/", 0);
		proc_ref(proc);
	}

	// add it to the global process list
	// note that the proc list only hold a weak ref
	proc_register(proc);
	
	spinlock_release(&proctree_lock);
	spinlock_release(&proc->proc_lock);

	return proc;
}

void proc_exit(int status) {
	spinlock_acquire(&get_current_proc()->proc_lock);
	get_current_proc()->exit_status = status;
	// mark the process as killed
	atomic_fetch_or(&get_current_proc()->flags, PROC_FLAG_KILLED);
	// interrupt every thread
	foreach (node, &get_current_proc()->threads) {
		task_t *task = container_of(node, task_t, thread_list_node);
		unblock_task_reason(task, WAKEUP_SIGNAL);
	}
	spinlock_release(&get_current_proc()->proc_lock);
	task_exit();
}

static void alert_parent(process_t *proc) {
	if (!proc->parent) return;
	siginfo_t siginfo = {
		.si_signo  = SIGCHLD,
		.si_code   = WIFEXITED(proc->exit_status) ? CLD_EXITED : CLD_KILLED,
		.si_status = proc->exit_status,
		.si_pid    = proc->pid,
		// TODO : uid ?
	};
	signal_send_siginfo_proc(proc->parent, &siginfo);
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
		if (proc_get_state(child) == PROC_STATE_ZOMBIE) alert_parent(child);
	}
	list_destroy(&get_current_proc()->child);

	// release session / group / cred
	proc_set_group(get_current_proc(), NULL);
	cred_release(get_current_cred());

	proc_set_state(get_current_proc(), PROC_STATE_ZOMBIE);

	spinlock_release(&proctree_lock);
	spinlock_release(&get_current_proc()->proc_lock);

	// close every open fd
	xarray_foreach (index, value, &get_current_proc()->fd_table) {
		(void)index;
		vfs_fd_t *fd = fd_value2fd(value, NULL);
		vfs_close(fd);
	}
	xarray_destroy(&get_current_proc()->fd_table);

	// release locked dentry
	vfs_dentry_release(get_current_proc()->cwd);
	vfs_dentry_release(get_current_proc()->exe);

	vmm_unmap_all();

	list_destroy(&get_current_proc()->threads);
	alert_parent(get_current_proc());
}

void proc_zombie_cleanup(process_t *proc) {
	spinlock_assert_acquired(&proctree_lock);
	proc_unregister(proc);
	if (proc->parent) list_remove(&proc->parent->child, &proc->child_list_node);

	// the parent hold a ref that we need to free
	proc_release(proc);
}

int fd_add(vfs_fd_t *fd, long flags) {
	if (IS_ERR(fd)) return PTR2ERR(fd);
	kassert(fd->ref_count > 0);
	
	uintptr_t value = (uintptr_t)fd;
	if (flags & FD_CLOEXEC) value |= FDTABLE_CLOEXEC;

	return xarray_allocate(&get_current_proc()->fd_table, (void*)value);
}

vfs_fd_t *fd_set(int index, vfs_fd_t *fd, long flags) {
	if (IS_ERR(fd)) return fd;
	kassert(fd->ref_count > 0);
	
	uintptr_t value = (uintptr_t)fd;
	if (flags & FD_CLOEXEC) value |= FDTABLE_CLOEXEC;

	void *prev_value = xarray_set(&get_current_proc()->fd_table, index, (void*)value);
	return fd_value2fd(prev_value, NULL);
}

vfs_fd_t *fd_get_flags(int fd, long *flags) {
	if (fd < 0) return NULL;

	rcu_acquire_read(&get_current_proc()->fd_table.rcu);
	vfs_fd_t *vfs_fd = fd_value2fd(xarray_get(&get_current_proc()->fd_table, fd), flags);
	if (vfs_fd) kassert(vfs_fd->ref_count > 0);
	vfs_dup(vfs_fd);
	rcu_release_read(&get_current_proc()->fd_table.rcu);
	return vfs_fd;
}


vfs_fd_t *fd_get_and_remove(int fd) {
	if (fd < 0) return NULL;

	return fd_value2fd(xarray_clear(&get_current_proc()->fd_table, fd), NULL);
}
