#ifndef SERIAL_H
#define SERIAL_H

//called serial for compatiblity reason
//acttually uart.h

int init_serial();
void write_serial_char(char c);
void write_serial(const char *str);

#endif
