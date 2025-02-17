#include "cmos.h"
#include <stdint.h>
#include "port.h"
#include "print.h"
#include <sys/time.h>

#define CMOS_ADDRESS_PORT 0x70
#define CMOS_DATA_PORT 0x71

#define CMOS_REGISTER_A 0x0A
#define CMOS_REGISTER_B 0x0B
#define CMOS_STATUS_BIT 0x80


void cmos_wait(){
	//wait until update start
	uint8_t status;
	do{
		out_byte(CMOS_ADDRESS_PORT,CMOS_REGISTER_A);
		status = in_byte(CMOS_DATA_PORT);
	}while(status & CMOS_STATUS_BIT);

	//wait until update finish
	do{
		out_byte(CMOS_ADDRESS_PORT,CMOS_REGISTER_A);
		status = in_byte(CMOS_DATA_PORT);
	}while(!(status & CMOS_STATUS_BIT));
}

uint8_t read_raw_cmos(uint8_t address){
	out_byte(CMOS_ADDRESS_PORT,address);
	return in_byte(CMOS_DATA_PORT);
}

static uint8_t mode;

uint8_t read_cmos(uint8_t address){
	uint8_t data = read_raw_cmos(address);
	if(!(mode & 0x04)){
		data = ((data / 16) * 10) + (data & 0x0f);
	}
	return data;
}

void init_cmos(void){
	kstatus("start reading cmos... ");
	cmos_wait();
	mode = read_raw_cmos(CMOS_REGISTER_B);
	uint8_t seconds = read_cmos(0x00);
	uint8_t minutes = read_cmos(0x02);
	uint8_t hours   = read_cmos(0x04);
	uint8_t day     = read_cmos(0x07);
	uint8_t month   = read_cmos(0x08);
	uint8_t years   = read_cmos(0x09);

	

	kok();
}