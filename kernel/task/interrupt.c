#include <kernel/interrupt.h>
#include <kernel/scheduler.h>
#include <kernel/signal.h>
#include <kernel/arch.h>

//generic intruppt handler

void timer_handler(fault_frame *frame) {
	yield(1);

	if (is_userspace(frame)) {
		handle_signal(frame);
	}
}

//called when a fault/interrupt occur in userspace
void fault_handler(fault_frame *frame) {
	if (get_ptr_context(frame) == MAGIC_SIGRETURN) {
		restore_signal_handler(frame);
	}
	//TODO : send appropriate signal
	send_sig_task(get_current_task(), SIGSEGV);

	if (is_userspace(frame)) {
		handle_signal(frame);
	}
}