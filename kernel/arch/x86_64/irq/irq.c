#include <kernel/arch.h>
#include <kernel/irq.h>
#include <kernel/kernel.h>
#include <kernel/print.h>

void init_arch_irq(void) {
	kstatusf("init irq chip... ");

	if (have_apic()) {
		init_apic();
	} else {
		init_pic();
	}

	kok();

	kinfof("using irq chip '%s'\n", main_irq_chip->name);
	enable_interrupt();
}
