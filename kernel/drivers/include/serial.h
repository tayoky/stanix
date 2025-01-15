#ifndef SERIAL_H
#define SERIAL_H

#define SERIAL_PORT 0X3F8

int init_serial(void);
void write_serial_char(const char data);
void write_serial(const char *string);

#endif