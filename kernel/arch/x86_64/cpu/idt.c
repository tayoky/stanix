#include <kernel/arch.h>
#include <kernel/interrupt.h>
#include <kernel/irq.h>
#include <kernel/kernel.h>
#include <kernel/panic.h>
#include <kernel/print.h>
#include <kernel/scheduler.h>
#include <kernel/userspace.h>
#include <kernel/signal.h>
#include <kernel/sys.h>
#include <stdint.h>

static idt_gate idt[256];
static IDTR idtr;

void safe_copy_fault(void);
void safe_copy_resolve_fault(void);

static void set_idt_gate(idt_gate *idt, uint8_t index, void *offset, uint8_t flags) {
	idt[index].offset1 = (uint64_t)offset & 0xFFFF;
	idt[index].offset2 = ((uint64_t)offset >> 16) & 0xFFFF;
	idt[index].offset3 = ((uint64_t)offset >> 32) & 0xFFFFFFFF;

	// make sure the present bit is set
	idt[index].flags    = flags | 0x80;
	idt[index].reserved = 0;
	idt[index].selector = 0x08;
}

static const char *error_msg[] = {
	"divide by zero",
	"debug",
	"non maskable",
	"breakpoint",
	"overflow",
	"bound range exceeded",
	"invalid OPcode",
	"device not available",
	"double fault",
	"Coprocessor segment overrun \n ask tayoky if you see this",
	"invalid tss",
	"segment not present",
	"stack segment fault",
	"general protection fault",
	"page fault",
	"not an error",
	"x87 floating point fault",
	"alignement check",
	"machine check",
	"SIMD floating point fault",
	"virtualization exception",
	"control protection exception",
};

static void page_fault_info(registers_t *fault) {
	kprintf("page fault at address 0x%lx\n", fault->cr2);
	if (fault->err_code & 0x04)
		kprintf("user");
	else
		kprintf("OS");
	kprintf(" has trying to ");
	if (fault->err_code & 0x10)
		kprintf("execute");
	else if (fault->err_code & 0x02)
		kprintf("write");
	else
		kprintf("read");
	kprintf(" a ");
	if (!(fault->err_code & 0x01)) kprintf("non ");
	kprintf("present page\n");
}

// TODO : move this to arch specific signal stuff
static void send_fault_signal(registers_t *registers) {
	siginfo_t siginfo = {0};
	switch (registers->err_type) {
	case 0:  // dividd error fault
	case 7:  // no coprocessor fault
	case 16: // coprocsessor fault
		siginfo.si_addr = (void*)registers->rip;
		siginfo.si_signo = SIGFPE;
		// TODO : fill si_code
		break;
	case 1:  // single step trap
	case 3:  // breakpoint trap
		siginfo.si_signo = SIGTRAP;
		break;
	case 2:  // non maskable interrupt
	case 8:  // double fault
	case 10: // invalid TSS fault
	case 11: // segment no present fault
		return;
	case 4:  // overflow trap
	case 9:  // coprocessor overrun abort
	case 12: // stack exception fault
	case 13: // general protection fault
		siginfo.si_signo = SIGSEGV;
		break;
	case 14: // page fault
		siginfo.si_addr = (void*)registers->cr2;
		siginfo.si_signo = SIGSEGV;
		break;
	case 6:
	default: // others
		siginfo.si_addr = (void*)registers->rip;
		siginfo.si_signo = SIGILL;
		break;
	}
	signal_send_siginfo_task(get_current_task(), &siginfo);
}

void isr_handler(registers_t *registers) {
	if (registers->err_type < 32) {
		if (registers->err_type == 14) {
			if (page_fault_handler(registers)) return;
		} else if (registers->err_type == 19) {
			if (fpu_fault_handler(registers)) return;
		}
			
		// special case for safe copy
		if (registers->err_type == 14 && registers->rip == (uintptr_t)safe_copy_fault) {
			registers->rip = (uintptr_t)safe_copy_resolve_fault;
			registers->rax = (uintptr_t)-EFAULT;
			return;
		}

		if (arch_registers_is_userspace(registers)) {
			send_fault_signal(registers);
			return_to_userspace(registers);
			return;
		}

		kprintf("error : 0x%lx\n", registers->err_type);
		if (registers->err_type < (sizeof(error_msg) / sizeof(char *))) {
			// show info for page fault
			if (registers->err_type == 14) {
				page_fault_info(registers);
			}
			panic(error_msg[registers->err_type], registers);
		} else {
			panic("unkown fault", registers);
		}
	} else if(registers->err_type == 0x80) {
		syscall_handler(registers, NULL);
	} else {
		irq_dispatch_vector(registers->err_type, registers);
	}
}

void init_idt(void) {
	kstatusf("init IDT... ");

// register exceptions handlers
#define X(name, i) set_idt_gate(idt, i, name, 0x8E);
	EXCEPTIONS();
#undef X

// irq
#define X(name, i) set_idt_gate(idt, i, name, 0xEE);
	IRQS();
#undef X

	// create the IDTR
	idtr.size   = sizeof(idt) - 1;
	idtr.offset = (uint64_t)&idt;
	// and load it
	asm("lidt %0" : : "m"(idtr));
	kok();
}
