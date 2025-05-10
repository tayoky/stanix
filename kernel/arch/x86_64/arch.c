#include <kernel/arch.h>
#include <kernel/serial.h>
#include "idt.h"
#include "gdt.h"
#include "tss.h"
#include "pit.h"
#include "cmos.h"

void kmain();

void _start(){
	//init arch sepcfic stuff before launching
	//generic main
	init_serial();
	init_gdt();
	init_idt();
	init_tss();

	kmain();
}

void init_timer(){
	init_cmos();
	init_pit();
}