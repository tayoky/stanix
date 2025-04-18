#ifndef PS2_H
#define PS2_H

#include <stdint.h>

//devices commands
#define PS2_DISABLE_SCANING 0xF5
#define PS2_ENABLE_SCANING  0xF4
#define PS2_IDENTIFY        0xF2
#define PS2_ACK             0xFA

int ps2_read(void);
int ps2_send(uint8_t port,uint8_t data);
void ps2_register_handler(void *handler,uint8_t port);

extern char ps2_have_port1;
extern char ps2_have_port2;

extern int ps2_port_id[3][2];

#endif