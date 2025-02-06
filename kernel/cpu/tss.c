#include "tss.h"
#include "kernel.h"
#include "print.h"
#include "string.h"
#include "paging.h"

void init_tss(){
	kstatus("init tss... ");

	//just set the tss
	memset(&kernel->tss,0,sizeof(TSS));
	kernel->tss.rsp0 = KERNEL_STACK_TOP;
	kdebugf("tss set now load it \n");
	//and load it
	asm("mov $0x28, %%ax\nltr %%ax" : : : "rax");

	kok();
}