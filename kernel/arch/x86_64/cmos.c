#include <kernel/arch.h>
#include <kernel/print.h>
#include <sys/time.h>
#include <stdint.h>

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

extern struct timeval time;

static int is_leap(uintmax_t year){
	//not divisible by 4
	if(year % 4){
		return 0;
	}

	//excecptioh for mutiple of 100 tha can't be divide by 400
	if((year % 400) && !(year % 100)){
		return 0;
	}

	return 1;
}

void init_cmos(void){
	kstatusf("start reading cmos... ");
	cmos_wait();
	mode = read_raw_cmos(CMOS_REGISTER_B);
	uint8_t seconds = read_cmos(0x00);
	uint8_t minutes = read_cmos(0x02);
	uint8_t hours   = read_cmos(0x04);
	uintmax_t day    = read_cmos(0x07);
	uint8_t month   = read_cmos(0x08);
	uintmax_t years  = read_cmos(0x09);

	//remove today day
	day--;

	if(years > 90){
		years += 1900;
	} else {
		years += 2000;
	}

	switch (month)
	{
	case 12:
		day += 30;
		//fallthrough
	case 11:
		day += 31;
		//fallthrough
	case 10:
		day += 30;
		//fallthrough
	case 9:
		day += 31;
		//fallthrough
	case 8:
		day += 31;
		//fallthrough
	case 7:
		day += 30;
		//fallthrough
	case 6:
		day += 31;
		//fallthrough
	case 5:
		day += 30;
		//fallthrough
	case 4:
		day += 31;
		//fallthrough
	case 3:
		if(is_leap(years)){
			day += 29;
		} else
		{
			day += 28;
		}
		//fallthrough
	case 2:
		day += 31;
		//fallthrough
	case 1:
		break;
	
	default:
		kfail();
		kinfof("can't read : month is %d\n",month);
		return;
		break;
	}

	//now convert years in day
	while(years > 1970){
		years--;
		if(is_leap(years)){
			day += 366;
		} else {
			day += 365;
		}
		
	}

	time.tv_sec = seconds + minutes * 60 + hours * 3600 + day * 86400;
	kok();

	kinfof("current time : %ld\n",time.tv_sec);
}