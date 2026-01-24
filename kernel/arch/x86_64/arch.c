#include <kernel/arch.h>
#include <kernel/serial.h>
#include <kernel/print.h>
#include <kernel/mmu.h>
#include <errno.h>

void kmain();

void _start(){
	// init arch sepcfic stuff before launching
	// generic main
	disable_interrupt();
	init_serial();
	init_gdt();
	init_idt();
	init_tss();
	enable_sse();

	kmain();
}

void init_timer(){
	init_cmos();
	init_pit();
}

int is_userspace(fault_frame *frame){
	return frame->cs == 0x1b;
}

uintptr_t get_ptr_context(fault_frame *fault){
	return fault->cr2;
}

long arch_get_prot_fault(fault_frame *fault) {
	if (fault->err_code & 0x10) return MMU_FLAG_EXEC;
	if (fault->err_code & 0x02) return MMU_FLAG_WRITE;
	return MMU_FLAG_READ;
}


void set_tls(void *tls){
	// set fs base
	asm volatile("wrmsr" : : "c"(0xc0000100), "d" ((uint32_t)(((uintptr_t)tls) >> 32)), "a" ((uint32_t)((uintptr_t)tls)));
}


int arch_shutdown(int flags){
	if (flags & SHUTDOWN_REBOOT) {
		// trigger a tripple fault
		IDTR zero_idtr = {
			.offset = 0,
			.size = 0,
		};
		asm("lidt %0 ; int $16" : : "m" (zero_idtr));
		__builtin_unreachable();
	} else {
		kdebugf("shutdown unimplemented\n");
		return -ENOSYS;
	}
}