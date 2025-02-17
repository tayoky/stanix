#include "pit.h"
#include "print.h"
#include "kernel.h"
#include "irq.h"
#include "port.h"
#include "serial.h"
#include "sleep.h"

#define PIT_CHANNEL0 0x40
#define PIT_CHANNEL1 0x41
#define PIT_CHANNEL2 0x42
#define PIT_COMMAND  0x43

void timer();

uint64_t sleep_tick = 0;

void init_pit(void){
	kstatus("init PIT... ");

	irq_map(timer,0);

	//the tick per second is defined here
	uint16_t tps = 1000;
	uint16_t divider = 1193181 / tps;

	out_byte(PIT_COMMAND,0b00110100);
	out_byte(PIT_CHANNEL0,divider & 0xFF);
	out_byte(PIT_CHANNEL0,(divider >> 8) & 0xFF);
	
	//just wait to make sure PIT work
	micro_sleep(100000);

	kok();
}