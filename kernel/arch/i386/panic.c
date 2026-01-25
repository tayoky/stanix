#include "panic.h"
#include <kernel/print.h>
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

void panic(const char *error,fault_frame_t *fault){
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
		kprintf("rax : 0x%p\n",fault->eax);
		kprintf("rbx : 0x%p\n",fault->ebx);
		kprintf("rcx : 0x%p\n",fault->ecx);
		kprintf("rdx : 0x%p\n",fault->edx);
		kprintf("rsi : 0x%p\n",fault->esi);
		kprintf("rdi : 0x%p\n",fault->edi);
		kprintf("ebp : 0x%p\n",fault->ebp);
		kprintf("rsp : 0x%p\n",fault->esp);
		kprintf("================== SPECIALS REGISTERS ==================\n");
		kprintf("cr2 : 0x%p\tcr3 : 0x%p\n",fault->cr2,fault->cr3);
		kprintf("rip : 0x%p\n",fault->rip);
		kprintf("cs  : 0x%p\tss  : 0x%p\n",fault->cs,fault->ss);
		kprintf("========================= FLAG =========================\n");
		kprintf("falgs: 0x%lx\n",fault->flags);
		kprintf("====================== STACK TRACE =====================\n");//NOT ALIGNED!!!!!
		kprintf("most recent call\n");
		kprintf("<0x%lx> %s\n",fault->rip,get_func_name(fault->rip));
		uintptr_t *ebp = (uintptr_t *)fault->ebp;
		while (ebp && *ebp && *(ebp+1)){
			kprintf("<0x%lx> %s\n",*(ebp+1),get_func_name(*(ebp+1)));
			ebp = (uintptr_t *)(*ebp);
		}
		
		kprintf("older call\n");

	}else{
		kprintf("unavalible\n");
		kprintf("===================== STACK TRACE ====================\n");//NOT ALIGNED!!!!!
		kprintf("most recent call\n");
		uintptr_t *ebp;
		asm("mov %%ebp , %%eax" : "=a" (ebp));
		while (ebp && *ebp && *(ebp+1)){
			kprintf("<0x%lx> %s\n",*(ebp+1),get_func_name(*(ebp+1)));
			ebp = (uintptr_t *)(*ebp);
		}
		kprintf("older call\n");
	}
	kprintf(COLOR_RESET);
	halt();
}