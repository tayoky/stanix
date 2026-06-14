#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <kernel/kheap.h>
#include <kernel/refcount.h>
#include <kernel/scheduler.h>
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

typedef struct process {
	list_node_t child_list_node;
	vmm_space_t vmm_space;
	ref_count_t ref_count;
	pid_t pid;
	struct process *parent;
	fd_table_t fd_table;
	vfs_dentry_t *cwd;
	vfs_dentry_t *exe;
	char *cmdline;
	uintptr_t heap_start;
	uintptr_t heap_end;
	list_t child;
	list_t threads;
	pid_t group;
	pid_t sid;
	uid_t uid;
	uid_t euid;
	uid_t suid;
	gid_t gid;
	gid_t egid;
	gid_t sgid;
	mode_t umask;
	task_t *main_thread;
	long exit_status;
} process_t;

static inline process_t *get_current_proc(void) {
	task_t *task = get_current_task();
	return task ? task->process : NULL;
}

static inline uid_t get_current_euid(void) {
	process_t *proc = get_current_proc();
	return proc ? proc->euid : EUID_ROOT;
}

static inline gid_t get_current_egid(void) {
	process_t *proc = get_current_proc();
	return proc ? proc->egid : EUID_ROOT;
}

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
process_t *pid2proc(pid_t pid);

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
	kfree(proc->cmdline);
	proc->cmdline = strdup(cmdline);
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

#endif
