#include "arch.h"
#include "idt.h"
#include "gdt.h"
#include "tss.h"

void kmain();

void _start(){
	//init arch sepcfic stuff before launching
	//generic main
	
	init_gdt();
	init_idt();
	init_tss();

	kmain();
}
