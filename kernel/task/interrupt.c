#include <kernel/interrupt.h>
#include <kernel/scheduler.h>
#include <kernel/signal.h>
#include <kernel/print.h>
#include <kernel/arch.h>
#include <kernel/vmm.h>

// generic interrupt handler

void timer_handler(registers_t *frame) {
	yield(1);

	if (arch_registers_is_userspace(frame)) {
		return_to_userspace(frame);
	}
}

// called on page faults
int page_fault_handler(registers_t *frame) {
	if (arch_registers_is_userspace(frame) && arch_fault_get_addr(frame) == MAGIC_SIGRETURN) {
		restore_signal_handler(frame);
	}

	// maybee the vmm can handle it
	if (vmm_fault_report(arch_fault_get_addr(frame), arch_fault_get_prot(frame))) {
		// even if the vmm handlded the fault
		// it does not handle return to userspace
		if (arch_registers_is_userspace(frame)) {
			return_to_userspace(frame);
		}
		return 1;
	}
	if (!arch_registers_is_userspace(frame)) {
		// at this point if it's kernel it's probably a panic
		return 0;
	}

	// print some info for debuging
	switch(arch_fault_get_prot(frame)) {
	case MMU_FLAG_READ:
		kdebugf("userspace (%lx) tryied to read %lx\n", PC_REG(*frame), arch_fault_get_addr(frame));
		break;
	case MMU_FLAG_WRITE:
		kdebugf("userspace (%lx) tryied to write %lx\n", PC_REG(*frame), arch_fault_get_addr(frame));
		break;
	case MMU_FLAG_EXEC:
		kdebugf("userspace (%lx) tryied to execute %lx\n", PC_REG(*frame), arch_fault_get_addr(frame));
		break;
	}

	// TODO : remove this
	arch_registers_stacktrace(frame);

	signal_send_task(get_current_task(), SIGSEGV);

	if (arch_registers_is_userspace(frame)) {
		return_to_userspace(frame);
	}
	return 1;
}

int fpu_fault_handler(registers_t *frame) {
	if (!arch_registers_is_userspace(frame)) {
		// if it's kernel it's probably a panic
		return 0;
	}

	signal_send_task(get_current_task(), SIGFPE);
	return_to_userspace(frame);
	return 1;
}
