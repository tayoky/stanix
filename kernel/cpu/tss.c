#include "tss.h"
#include "kernel.h"
#include "print.h"
#include "string.h"
#include "paging.h"

void init_tss(){
	kstatus("init tss... ");

	//just set the tss
	memset(&kernel->tss,0,sizeof(TSS));
	kernel->tss.rsph0 = (KERNEL_STACK_TOP - 8) >> 32;
	kernel->tss.rspl0 = (KERNEL_STACK_TOP - 8);


	//and load it
	asm("mov $0x28, %%ax\nltr %%ax" : : : "rax");

	kok();
}