#include "sys.h"
#include "scheduler.h"
#include "print.h"
#include "kernel.h"
#include "idt.h"

extern void syscall_handler();

void init_syscall(void){
	kstatus("init syscall... ");
	set_idt_gate(kernel->idt,0x80,syscall_handler,0xEF);
	kok();
}

void sys_exit(uint64_t error_code){
	kdebugf("exit with code : %ld\n",error_code);
	kill_proc(get_current_proc());
}

pid_t sys_getpid(){
	return get_current_proc()->pid;
}