#include "port.h"
#include "serial.h"
#include <stdint.h>

int init_serial(void){
	out_byte(SERIAL_PORT + 1, 0x00);
	out_byte(SERIAL_PORT + 3, 0x80);
	out_byte(SERIAL_PORT + 0, 0x03);
	out_byte(SERIAL_PORT + 1, 0x00);
	out_byte(SERIAL_PORT + 3, 0x03);
	out_byte(SERIAL_PORT + 2, 0xC7);
	out_byte(SERIAL_PORT + 4, 0x0B);
	out_byte(SERIAL_PORT + 4, 0x1E);
	out_byte(SERIAL_PORT + 0, 0xAE);
	if(in_byte(SERIAL_PORT + 0) != 0xAE) {
		return 1;
	}
	out_byte(SERIAL_PORT + 4, 0x0F);
	return 0;
}

void write_serial_char(const char data){
	while (!(in_byte(SERIAL_PORT + 5) & 0x20));
	out_byte(SERIAL_PORT,data);
}

void write_serial(const char *string){
	uint64_t i = 0;
	while (string[i]){
		write_serial_char(string[i]);
		i ++;
	}
	
}