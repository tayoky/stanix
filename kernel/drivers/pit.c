#include "pit.h"
#include "print.h"
#include "kernel.h"
#include "irq.h"
#include "port.h"

#define PIT_CHANNEL0 0x40
#define PIT_CHANNEL1 0x41
#define PIT_CHANNEL2 0x42
#define PIT_COMMAND  0x43

void timer();

void init_pit(void){
	kstatus("init PIT... ");

	irq_map(timer,0);
	uint16_t divider = 0xBBBB;

	out_byte(PIT_COMMAND,0b00110100);
	out_byte(PIT_CHANNEL0,divider & 0xFF);
	out_byte(PIT_CHANNEL0,(divider >> 8) & 0xFF);

	kok();
}