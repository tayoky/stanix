#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <kernel/kheap.h>
#include <kernel/refcount.h>
#include <kernel/rculist.h>
#include <kernel/scheduler.h>
#include <kernel/sleep.h>
#include <kernel/cred.h>
#include <kernel/vfs.h>
#include <kernel/vmm.h>
#include <sys/signal.h>

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

typedef struct process process_t;
struct tty;

typedef struct session {
	rculist_t groups;
	ref_count_t ref_count;
	process_t *leader; // protected by lock
	struct tty *tty;   // protected by lock
	spinlock_t lock;
	pid_t sid;
} session_t;

typedef struct process_group {
	rculist_node_t node;
	rculist_t processes;  // write protected by proctree lock
	ref_count_t ref_count;
	session_t *session;
	pid_t pgid;
} process_group_t;

struct process {
	list_node_t child_list_node; // protected by proctree lo k
	rculist_node_t group_node;   // write protected by proctree lock
	vmm_space_t vmm_space;
	sleep_queue_t wait_queue;
	signal_context_t signal_context;   // cannot acquire if holding proc lock
	struct sigaction sig_handlers[32]; // protected by proc lock
	rcu_ptr_t cred;              // write protected by proc lock
	ref_count_t ref_count;
	process_t *parent;           // write protected by proctree lock and protected by proc lock
	process_group_t *group;      // write protected by proctree lock and protected by proc lock
	fd_table_t fd_table;
	vfs_dentry_t *cwd;
	vfs_dentry_t *exe;
	char *cmdline;               // protected by proc lock
	uintptr_t heap_start;
	uintptr_t heap_end;
	list_t child;                // protected by proctree lock
	list_t threads;              // protected by proc lock
	size_t threads_count;        // protected by proc lock
	pid_t pid;
	mode_t umask;
	spinlock_t proc_lock;        // cannot be acquired if holding proctree lock
	task_t *main_thread;
	int exit_status;             // write protected by proc lock
	ATOMIC(int) state;           // write protected by proc lock and proctree lock
	ATOMIC(int) flags;
};

#define PROC_STATE_RUNNING 1
#define PROC_STATE_ZOMBIE  2

#define PROC_FLAG_KILLED  0x1 // process is killed

void init_proc(void);

static inline session_t *session_ref(session_t *session) {
	if (session) ref_count_inc(&session->ref_count);
	return session;
}

void session_release(session_t *session);
int session_create(process_t *leader);

static inline process_group_t *process_group_ref(process_group_t *group) {
	if (group) ref_count_inc(&group->ref_count);
	return group;
}

void process_group_release(process_group_t *group);

process_group_t *process_group_from_pgid(pid_t pgid);
process_group_t *process_group_create(pid_t *pgid);

void process_group_set_session(process_group_t *group, session_t *session);

/**
 * @note require proc's lock and the proctree_lock
 */
int proc_set_group(process_t *proc, process_group_t *group);

static inline cred_t *proc_get_cred(process_t *proc) {
	if (!proc) return NULL;
	return rcu_ptr_fetch(&proc->cred);
}

/**
 * @note require the proc's lock
 */
static inline void proc_set_cred(process_t *proc, cred_t *cred) {
	spinlock_assert_acquired(&proc->proc_lock);
	cred_t *old_cred = rcu_ptr_store(&proc->cred, cred_ref(cred));
	rcu_sync(NULL);
	cred_release(old_cred);
}

static inline int proc_get_state(process_t *proc) {
	return atomic_load(&proc->state);
}

/**
 * @note require the proc's lock
 */
static inline void proc_set_state(process_t *proc, int state) {
	spinlock_assert_acquired(&proc->proc_lock);
	atomic_store(&proc->state, state);
	if (proc->parent) wakeup_queue(&proc->parent->wait_queue, 0);
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
	static inline type proc_get_ ## var(process_t *proc) {\
		return proc_get_cred(proc)->var;\
	}\
	static inline type get_current_ ## var(void) {\
		rcu_acquire_read(NULL);\
		type var = get_current_cred()->var;\
		rcu_release_read(NULL);\
		return var;\
	}

CRED_HELPER(uid_t, uid)
CRED_HELPER(uid_t, euid)
CRED_HELPER(uid_t, suid)
CRED_HELPER(gid_t, gid)
CRED_HELPER(gid_t, egid)
CRED_HELPER(gid_t, sgid)

/**
 * @brief allocate a process struct
 * @return the newly allocate process struct
 */
process_t *proc_allocate(void);

process_t *proc_new(void (*func)(void *arg), void *arg);

/**
 * @brief kill the current process
 * @param status the status to exit with
 */
void proc_exit(int status);

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
 * @brief do deletion of the current process
 */
void do_proc_deletion(void);

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
 * @brief register a process into the process list
 * @param proc the process to register
 */
void proc_register(process_t *proc);

/**
 * @brief cleanup a zombie process
 * @param proc the zombie to cleanup
 * @note require the proctree lock
 */
void proc_zombie_cleanup(process_t *proc);

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

extern xarray_t procs;
extern process_t *init;
extern spinlock_t proctree_lock;

#endif
