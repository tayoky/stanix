#include "tss.h"
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/string.h>
#include <kernel/paging.h>

void arch_set_kernel_stack(uintptr_t stack){
	kernel->arch.tss.rsph0 = ((stack) >> 32) & 0xFFFFFFFF;
	kernel->arch.tss.rspl0 = (stack) & 0xFFFFFFFF;
}

void init_tss(){
	kstatusf("init tss... ");

	//just set the tss
	memset(&kernel->arch.tss,0,sizeof(TSS));

	//and load it
	asm("mov $0x28, %%ax\nltr %%ax" : : : "rax");

	kok();
}