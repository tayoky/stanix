#include <kernel/arch.h>
#include <kernel/mmu.h>
#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/sym.h>

extern uint64_t p_kernel_end[];

void arch_registers_dump(registers_t *registers) {
	static acontext_t current_context;
	if (!registers) {
		arch_save_context(&current_context);
		registers = &current_context.frame;
	}
    kprintf("==================== REGISTERS DUMP ====================\n");
	kprintf("rax : 0x%p\tr8  : 0x%p\n", registers->rax, registers->r8);
	kprintf("rbx : 0x%p\tr9  : 0x%p\n", registers->rbx, registers->r9);
	kprintf("rcx : 0x%p\tr10 : 0x%p\n", registers->rcx, registers->r10);
	kprintf("rdx : 0x%p\tr11 : 0x%p\n", registers->rdx, registers->r11);
	kprintf("rsi : 0x%p\tr12 : 0x%p\n", registers->rsi, registers->r12);
	kprintf("rdi : 0x%p\tr13 : 0x%p\n", registers->rdi, registers->r13);
	kprintf("rbp : 0x%p\tr14 : 0x%p\n", registers->rbp, registers->r14);
	kprintf("rsp : 0x%p\tr15 : 0x%p\n", registers->rsp, registers->r15);
	kprintf("================== SPECIAL REGISTERS ===================\n");
	kprintf("cr2 : 0x%p\n", registers->cr2);
	kprintf("rip : 0x%p\n", registers->rip);
	kprintf("cs  : 0x%p\tss  : 0x%p\n", registers->cs, registers->ss);
	kprintf("es  : 0x%p\tds  : 0x%p\n", registers->es, registers->ds);
	kprintf("gs  : 0x%p\tfs  : 0x%p\n", registers->gs, registers->fs);
	kprintf("========================= FLAG =========================\n");
	kprintf("flags: 0x%lx\n", registers->flags);
}

static const char *get_func_name(uintptr_t addr) {
	uintptr_t best_match = 0;
	const char *name     = "";
	for (size_t i = 0; i < symbols_count; i++) {
		if (symbols[i].value <= addr && symbols[i].value > best_match) {
			best_match = symbols[i].value;
			name       = symbols[i].name;
		}
	}

	// search in the sym list of modules and after kernel end
	if (addr > (uintptr_t)&p_kernel_end && exported_sym_list) {
		exported_sym *current = exported_sym_list;
		while (current) {
			if (current->value <= addr && current->value > best_match) {
				best_match = current->value;
				name       = current->name;
			}
			current = current->next;
		}
	}

	return name;
}

void arch_registers_stacktrace(registers_t *registers) {
	kprintf("====================== STACK TRACE =====================\n");
	kprintf("most recent call\n");
	if (registers) kprintf("<0x%lx> %s\n", registers->rip, get_func_name(registers->rip));
	uint64_t *rbp;
	if (registers) {
		rbp = (uint64_t *)registers->rbp;
	} else {
		asm("movq %%rbp, %0" : "=r"(rbp));
	}
	while (rbp && *rbp && *(rbp + 1)) {
		kprintf("<0x%lx> %s\n", *(rbp + 1), get_func_name(*(rbp + 1)));
		rbp = (uint64_t *)(*rbp);
	}

	kprintf("older call\n");
}

int arch_registers_is_userspace(registers_t *registers) {
	return registers->cs == 0x1b;
}

uintptr_t arch_fault_get_addr(registers_t *fault) {
	return fault->cr2;
}

long arch_fault_get_prot(registers_t *fault) {
	if (fault->err_code & 0x10) return MMU_FLAG_EXEC;
	if (fault->err_code & 0x02) return MMU_FLAG_WRITE;
	return MMU_FLAG_READ;
}
