#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <kernel/kheap.h>
#include <kernel/refcount.h>
#include <kernel/scheduler.h>
#include <kernel/cred.h>
#include <kernel/vfs.h>
#include <kernel/vmm.h>

#define MAX_FD 32

typedef struct file_descriptor {
	vfs_fd_t *fd;
	long present;
	long flags;
} file_descriptor_t;

typedef struct fd_table {
	file_descriptor_t fds[MAX_FD];
	rwlock_t lock;
} fd_table_t;

typedef struct process_group {
	rculist_t processes;
	ref_count_t ref_count;
	pid_t pgid;
} process_group_t;

struct process {
	list_node_t child_list_node; // protected by proctree lo k
	rculist_node_t group_node;   // write protected by proctree lock
	vmm_space_t vmm_space;
	rcu_ptr_t cred;
	ref_count_t ref_count;
	process_t *parent;           // write protected by proctree lock and read protected by proc_lock
	process_group_t *group;      // write protected by proctree lock and read protected by proc_lock
	fd_table_t fd_table;
	vfs_dentry_t *cwd;
	vfs_dentry_t *exe;
	char *cmdline;              // protected by proc_lock
	uintptr_t heap_start;
	uintptr_t heap_end;
	list_t child;
	list_t threads;
	pid_t pid;
	pid_t sid;
	mode_t umask;
	spinlock_t proc_lock; // cannot be acquired if holding proctree
	task_t *main_thread;
	int exit_status;
};

void init_proc(void);

static inline process_group_t *process_group_ref(process_group_t *group) {
	if (group) ref_count_inc(&group->ref_count);
	return group;
}

void process_group_release(process_group_t *group);

process_group_t *process_group_get_from_pgid(pid_t pgid);
process_group_t *process_group_get_or_create_from_pgid(pid_t pgid);

/**
 * @note require proc's proc_lock and the proctree_lock
 */
void proc_set_group(process_t *proc, process_group_t *group);

static inline cred_t *proc_get_cred(proc_t *proc) {
	if (!proc) return NULL;
	return rcu_ptr_fetch(&proc->cred);
}

static inline void proc_set_cred(proc_t *proc, cred_t *cred) {
	spinlock_assert_acquired(&proc->proc_lock);
	cred_t *old_cred = rcu_ptr_store(&proc->cred, cred_ref(cred));
	rcu_sync();
	cred_release(old_cred);
}

static inline process_t *get_current_proc(void) {
	task_t *task = get_current_task();
	return task ? task->process : NULL;
}

static inline cred_t *get_current_cred(void) {
	cred_t *cred = proc_get_cred(get_current_proc());
	if (!cred) cred = &default_cred;
	return cred;
}

static inline void set_current_cred(cred_t *cred) {
	proc_set_cred(get_current_proc(), cred);
}

#define CRED_HELPER(type, var) \
	static inline type proc_get_ ## var(proc_t *proc) {\
		return proc_get_cred(proc)->var;\
	}\
	static inline type get_current_ ## var(void) {\
		rcu_acquire_read(NULL);\
		type var = get_current_cred()->var;\
		rcu_release_read(NULL);\
		return var\
	}

CRED_HELPER(uid_t, uid)
CRED_HELPER(uid_t, euid)
CRED_HELPER(uid_t, suid)
CRED_HELPER(gid_t, gid)
CRED_HELPER(gid_t, egid)
CRED_HELPER(gid_t, sgid)

process_t *new_proc(void (*func)(void *arg), void *arg);

/**
 * @brief kill the current process
 */
void kill_proc();

/**
 * @brief get a process from its pid
 * @param pid the pid of the process
 * @return the process with the specfied pid
 * @note this create a new reference to the process
 */
process_t *proc_from_pid(pid_t pid);

/**
 * @brief increment the ref count of a process
 * @param proc the process to increment the ref count of
 * @return the process
 */
static inline process_t *proc_ref(process_t *proc) {
	if (proc) ref_count_inc(&proc->ref_count);
	return proc;
}

/**
 * @brief release a reference to a process
 * @param proc the process to release
 */
void proc_release(process_t *proc);

/**
 * @brief set cmdline of a process
 * @param proc the process to set the cmdline of
 * @param cmdline the new cmdline
 */
static inline void proc_set_cmdline(process_t *proc, const char *cmdline) {
	spinlock_acquire(&proc->proc_lock);
	kfree(proc->cmdline);
	proc->cmdline = strdup(cmdline);
	spinlock_release(&proc->proc_lock);
}

/**
 * @brief set cmdline of current process
 * @param cmdline the new cmdline
 */
static inline void set_cmdline(const char *cmdline) {
	proc_set_cmdline(get_current_proc(), cmdline);
}

/**
 * @brief add a file descriptor to the current's process fd table
 * @param fd the \ref vfs_fd_t to add
 * @param flags the fd flags to add with (FD_CLOEXEC, ...)
 * @return the fd number on success else an error code
 */
int add_fd(vfs_fd_t *fd, long flags);

/**
 * @brief get a file descriptor from the current's process fd table
 * @param fd the fd number to get the data of
 * @param file_descriptor where to store the fetched data (can be NULL)
 * @return the fd number on success else an error code
 */
int get_fd(int fd, file_descriptor_t *file_descriptor);

/**
 * @brief remove and close a file descriptor from the current's process fd table
 * @param fd the fd number to close
 * @return the fd number on succes else an error code
 */
int close_fd(int fd);

struct xarray;
struct xarray *get_procs_list(void);
extern spinlock_t proctree_lock;

#endif
