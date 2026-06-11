#include <kernel/arch.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/panic.h>
#include <kernel/print.h>
#include <kernel/earlycon.h>
#include <kernel/scheduler.h>
#include <kernel/sym.h>

int panic_count = 0;

extern uint64_t p_kernel_end[];

void panic(const char *error, registers_t *fault) {
	disable_interrupt();
	panic_count++;
	if (panic_count > 1) {
		earlycon_output_str("\nkernel panic paniced\n");
		halt();
	}
	pid_t pid           = 0;
	uintptr_t stack_top = 0;
	if (get_current_task()) {
		pid       = get_current_task()->tid;
		stack_top = KSTACK_TOP(get_current_task()->kernel_stack);
	}
	kprintf(COLOR_RED "================= ERROR : KERNEL PANIC =================\n");
	kprintf("error : %s\n", error);
	if (fault) kprintf("code : 0x%lx\n", fault->err_code);
	kprintf("========================= INFO =========================\n");
	kprintf("pid : %ld\tstack top : 0x%p\n", pid, stack_top);

	if (fault) {
		arch_registers_dump(fault);
	} else {
		kprintf("==================== REGISTERS DUMP ====================\n");
		kprintf("unavalible\n");
	}
	arch_registers_stacktrace(fault);

	kprintf(COLOR_RESET);
	halt();
}
