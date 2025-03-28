#include "tss.h"
#include "kernel.h"
#include "print.h"
#include "string.h"
#include "paging.h"

void init_tss(){
	kstatus("init tss... ");

	//just set the tss
	memset(&kernel->arch.tss,0,sizeof(TSS));
	kernel->arch.tss.rsph0 = (KERNEL_STACK_TOP) >> 32;
	kernel->arch.tss.rspl0 = (KERNEL_STACK_TOP);


	//and load it
	asm("mov $0x28, %%ax\nltr %%ax" : : : "rax");

	kok();
}