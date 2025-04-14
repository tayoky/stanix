#include "panic.h"
#include "print.h"
#include "asm.h"
#include <kernel/kernel.h>
#include <kernel/scheduler.h>
#include "serial.h"
#include "sym.h"

int panic_count = 0;

static char *get_func_name(uintptr_t addr){
	uintptr_t best_match = (uintptr_t)0;
	char * name = "";
	for (size_t i = 0; i < symbols_count; i++){
		if(symbols[i].value <= addr && symbols[i].value > best_match){
			best_match = symbols[i].value;
			name = symbols[i].name;
		}
	}
	
	return name;
}

void panic(const char *error,fault_frame *fault){
	disable_interrupt();
	panic_count++;
	if(panic_count > 1){
		write_serial("\nkernel panic paniced\n");
		halt();
	}
	pid_t pid = 0;
	if(kernel->can_task_switch){
		pid = get_current_proc()->pid;
	}
	kprintf(COLOR_RED "================= ERROR : KERNEL PANIC =================\n");
	kprintf("error : %s\n",error);
	if(fault)kprintf("code : 0x%lx\n",fault->err_code);
	kprintf("========================= INFO =========================\n");
	kprintf("pid : %ld\n",pid);
	kprintf("==================== REGISTERS DUMP ====================\n");
	if(fault){
		kprintf("rax : 0x%p\tr8  : 0x%p\n",fault->rax,fault->r8);
		kprintf("rbx : 0x%p\tr9  : 0x%p\n",fault->rbx,fault->r9);
		kprintf("rcx : 0x%p\tr10 : 0x%p\n",fault->rcx,fault->r10);
		kprintf("rdx : 0x%p\tr11 : 0x%p\n",fault->rdx,fault->r11);
		kprintf("rsi : 0x%p\tr12 : 0x%p\n",fault->rsi,fault->r12);
		kprintf("rdi : 0x%p\tr13 : 0x%p\n",fault->rdi,fault->r13);
		kprintf("rbp : 0x%p\tr14 : 0x%p\n",fault->rbp,fault->r14);
		kprintf("rsp : 0x%p\tr15 : 0x%p\n",fault->rsp,fault->r15);
		kprintf("================== SPECIALS REGISTERS ==================\n");
		kprintf("cr2 : 0x%p\tcr3 : 0x%p\n",fault->cr2,fault->cr3);
		kprintf("rip : 0x%p\n",fault->rip);
		kprintf("========================= FLAG =========================\n");
		kprintf("falgs: 0x%lx\n",fault->flags);
		kprintf("====================== STACK TRACE =====================\n");//NOT ALIGNED!!!!!
		kprintf("most recent call\n");
		kprintf("<0x%lx> %s\n",fault->rip,get_func_name(fault->rip));
		uint64_t *rbp = (uint64_t *)fault->rbp;
		while (rbp && *rbp && *(rbp+1)){
			kprintf("<0x%lx> %s\n",*(rbp+1),get_func_name(*(rbp+1)));
			rbp = (uint64_t *)(*rbp);
		}
		
		kprintf("older call\n");

	}else{
		kprintf("unavalible\n");
		kprintf("===================== STACK TRACE ====================\n");//NOT ALIGNED!!!!!
		kprintf("most recent call\n");
		uint64_t *rbp;
		asm("mov %%rbp , %%rax" : "=a" (rbp));
		while (rbp && *rbp && *(rbp+1)){
			kprintf("<0x%lx> %s\n",*(rbp+1),get_func_name(*(rbp+1)));
			rbp = (uint64_t *)(*rbp);
		}
		kprintf("older call\n");
	}
	kprintf(COLOR_RESET);
	halt();
}