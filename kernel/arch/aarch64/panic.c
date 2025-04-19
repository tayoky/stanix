#include "panic.h"
#include <kernel/print.h>
#include "asm.h"
#include "serial.h"

int panic_count = 0;

void panic(const char *error,fault_frame *fault){
	panic_count ++;
	if(panic_count > 2){
		halt();
	}
	if(panic_count > 1){
		write_serial("kernel panic panicked\n");
		halt();
	}
	disable_interrupt();

	kprintf("KERNEL PANIC\n");
	halt();
}
