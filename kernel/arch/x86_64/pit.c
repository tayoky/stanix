#include "pit.h"
#include <kernel/print.h>
#include <kernel/kernel.h>
#include "irq.h"
#include "port.h"
#include "serial.h"
#include "time.h"
#include <kernel/arch.h>

#define PIT_CHANNEL0 0x40
#define PIT_CHANNEL1 0x41
#define PIT_CHANNEL2 0x42
#define PIT_COMMAND  0x43
#define TPS 100

void pit_handler(fault_frame *frame){
	//update the time
	time.tv_usec += 1000000/TPS;
	if(time.tv_usec >= 1000000){
		time.tv_usec -= 1000000;
		time.tv_sec++;
	}

	irq_eoi(frame->err_code);
	frame->err_code = (uint64_t)-1;

	//yeld to next task
	yeld();
}

void init_pit(void){
	kstatus("init PIT... ");

	//the tick per second is defined here
	uint16_t divider = 1193181 / TPS;

	out_byte(PIT_COMMAND,0b00110100);
	out_byte(PIT_CHANNEL0,divider & 0xFF);
	out_byte(PIT_CHANNEL0,(divider >> 8) & 0xFF);

	irq_generic_map(pit_handler,0);
	
	//just wait to make sure PIT work
	micro_sleep(100000);

	kok();
}