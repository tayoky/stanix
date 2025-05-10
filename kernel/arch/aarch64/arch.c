#include <kernel/serial.h>
void kmain();

void _start(){
	init_serial();
	kmain();
}