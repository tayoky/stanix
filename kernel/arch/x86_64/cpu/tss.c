#include "tss.h"
#include <kernel/kernel.h>
#include "print.h"
#include "string.h"
#include <kernel/paging.h>

void init_tss(){
	kstatus("init tss... ");

	//just set the tss
	memset(&kernel->arch.tss,0,sizeof(TSS));
	kernel->arch.tss.rsph0 = ((KERNEL_STACK_TOP) >> 32) & 0xFFFFFFFF;
	kernel->arch.tss.rspl0 = (KERNEL_STACK_TOP) & 0xFFFFFFFF;


	//and load it
	asm("mov $0x28, %%ax\nltr %%ax" : : : "rax");

	kok();
}