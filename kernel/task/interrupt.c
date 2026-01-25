#include <kernel/interrupt.h>
#include <kernel/scheduler.h>
#include <kernel/signal.h>
#include <kernel/arch.h>
#include <kernel/vmm.h>

// generic interrupt handler

void timer_handler(fault_frame_t *frame) {
	yield(1);

	if (is_userspace(frame)) {
		handle_signal(frame);
	}
}

//called when a fault/interrupt occur in userspace
void fault_handler(fault_frame_t *frame) {
	if (arch_get_fault_addr(frame) == MAGIC_SIGRETURN) {
		restore_signal_handler(frame);
	}

	// maybee the vmm can handle it
	if (vmm_fault_report(arch_get_fault_addr(frame), arch_get_fault_prot(frame))) {
		// even if the vmm handlded the fault
		// it does not handle signals
		if (is_userspace(frame)) {
			handle_signal(frame);
		}
		return;
	}
	//TODO : send appropriate signal
	send_sig_task(get_current_task(), SIGSEGV);

	if (is_userspace(frame)) {
		handle_signal(frame);
	}
}