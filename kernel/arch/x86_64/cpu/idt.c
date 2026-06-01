#include <kernel/arch.h>
#include <kernel/interrupt.h>
#include <kernel/irq.h>
#include <kernel/kernel.h>
#include <kernel/panic.h>
#include <kernel/print.h>
#include <kernel/scheduler.h>
#include <kernel/signal.h>
#include <kernel/sys.h>
#include <stdint.h>

static interrupt_handler_t handlers[256];
static void *handlers_data[256];
static irqnum_t irqs[256];

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
	"device not avalibe",
	"double fault",
	"Coprocessor segment overrun \n ask tayoky if you see this",
	"invalid tss",
	"segment not present",
	"stack segment fault",
	"general protection fault",
	"page fault",
	"not an error",
	"x87 floating point fault",
	"alginement check",
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

void isr_handler(registers_t *registers) {
	if (registers->err_type < 32) {
		if (fault_handler(registers)) return;

		// special case for safe copy
		if (registers->err_type == 14 && registers->rip == (uintptr_t)safe_copy_fault) {
			registers->rip = (uintptr_t)safe_copy_resolve_fault;
			registers->rax = (uintptr_t)-EFAULT;
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
	} else {
		interrupt_handler_t handler = handlers[registers->err_type];
		void *data                  = handlers_data[registers->err_type];
		irqnum_t irq_num            = irqs[registers->err_type];
		if (irq_num >= 0) {
			irq_eoi(irq_num);
		}
		handler(registers, data);
	}
}

void init_idt(void) {
	kstatusf("init IDT... ");

// register exceptions handlers
#define X(name, i) set_idt_gate(kernel->arch.idt, i, name, 0x8E);
	EXCEPTIONS();
#undef X

// irq
#define X(name, i) set_idt_gate(kernel->arch.idt, i, name, 0xEE);
	IRQS();
#undef X

	idt_register_handler(0x80, syscall_handler, NULL, -1);

	// create the IDTR
	kernel->arch.idtr.size   = sizeof(kernel->arch.idt) - 1;
	kernel->arch.idtr.offset = (uint64_t)&kernel->arch.idt;
	// and load it
	asm("lidt %0" : : "m"(kernel->arch.idtr));
	kok();
}


void idt_register_handler(int vector, void *handler, void *data, irqnum_t irq_num) {
	// save the handler
	handlers[vector]      = handler;
	handlers_data[vector] = data;
	irqs[vector]          = irq_num;
}

int idt_allocate(void *handler, void *data, irqnum_t irq_num) {
	int i = 64;
	while (i < 256 - 32) {
		if (!handlers[i]) {
			handlers[i] = handler;
			idt_register_handler(i, handler, data, irq_num);
			return i;
		}
		i++;
	}
	return -1;
}
