#include <kernel/arch.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/string.h>

TSS tss;

void arch_set_kernel_stack(uintptr_t stack) {
	tss.rsph0 = ((stack) >> 32) & 0xFFFFFFFF;
	tss.rspl0 = (stack) & 0xFFFFFFFF;
}

void init_tss() {
	kstatusf("init tss... ");

	// just set the tss
	memset(&tss, 0, sizeof(TSS));

	// and load it
	asm("mov $0x28, %%ax\nltr %%ax" : : : "rax");

	kok();
}
