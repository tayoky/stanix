#include <kernel/arch.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/panic.h>
#include <kernel/print.h>
#include <kernel/earlycon.h>
#include <kernel/scheduler.h>
#include <kernel/atomic.h>
#include <kernel/sym.h>

static ATOMIC(int) panic_count = 0;

void panic(const char *error, registers_t *fault) {
	disable_interrupt();
	if (atomic_fetch_add(&panic_count, 1) > 0) {
		earlycon_output_str("\nkernel panic panicked\n");
		halt();
	}
	pid_t tid           = 0;
	uintptr_t stack_top = 0;
	if (get_current_task()) {
		tid       = get_current_task()->tid;
		stack_top = KSTACK_TOP(get_current_task()->kernel_stack);
	}
	kprintf(COLOR_RED "================= ERROR : KERNEL PANIC =================\n");
	kprintf("error : %s\n", error);
	if (fault) kprintf("code : 0x%lx\n", fault->err_code);
	kprintf("========================= INFO =========================\n");
	kprintf("tid : %ld\tstack top : 0x%p\n", tid, stack_top);

	if (fault) {
		arch_registers_dump(fault);
	} else {
		kprintf("==================== REGISTERS DUMP ====================\n");
		kprintf("unavailable\n");
	}
	arch_registers_stacktrace(fault);

	kprintf(COLOR_RESET);
	halt();
}
