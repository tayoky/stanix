#ifndef KERNEL_SERIAL_H
#define KERNEL_SERIAL_H

#define SERIAL_PORT 0X3F8

int init_serial(void);
void init_vbox_log(void);

#endif