#include <kernel/fork.h>
#include <kernel/arch.h>
#include <kernel/process.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/arch.h>
#include <kernel/vmm.h>

typedef struct fork_data {
	registers_t registers;
	int kill;
} fork_data_t;

static void fork_trampoline(void *arg) {
	fork_data_t data = *(fork_data_t *)arg;
	kfree(arg);

	if (data.kill) return;
	arch_registers_load(&data.registers);
}

pid_t fork(void) {
	// setup new context for child
	// and return 0 to the child
	fork_data_t *data = kmalloc(sizeof(fork_data_t));
	if (!data) return -ENOMEM;
	data->registers = *get_current_task()->syscall_frame;
	data->kill = 0;
	RET_REG(data->registers) = 0;

	process_t *parent = get_current_proc();
	process_t *child = proc_new(fork_trampoline, data);
	if (!child) {
		kfree(data);
		return -ENOMEM;
	}

	kdebugf("forking child : %ld\n", child->pid);

	int ret = vmm_clone(&parent->vmm_space, &child->vmm_space);
	if (ret < 0) {
		data->kill = 1;
		unblock_task(child->main_thread);
		return ret;
	}

	// clone metadata
	child->heap_end   = parent->heap_end;
	child->heap_start = parent->heap_start;
	memcpy(child->sig_handlers, get_current_proc()->sig_handlers, sizeof(get_current_proc()->sig_handlers));

	// clone fd table
	xarray_acquire_read(&get_current_proc()->fd_table);
	xarray_foreach (index, value, &get_current_proc()->fd_table) {
		vfs_fd_t *fd = fd_value2fd(value, NULL);
		vfs_dup(fd);
		xarray_release_read(&get_current_proc()->fd_table);
		xarray_set(&child->fd_table, index, value);
		xarray_acquire_read(&get_current_proc()->fd_table);
	}
	xarray_release_read(&get_current_proc()->fd_table);

	// make it ruuuunnnnn !!!
	unblock_task(child->main_thread);

	return child->pid;
}
