#ifndef PS2_H
#define PS2_H

#include <kernel/bus.h>
#include <stdint.h>

//devices commands
#define PS2_DISABLE_SCANING 0xF5
#define PS2_ENABLE_SCANING  0xF4
#define PS2_IDENTIFY        0xF2
#define PS2_ACK             0xFA

typedef int ps2_device_id_t[2];

typedef struct ps2_addr {
	bus_addr_t addr;
	uint8_t port;
	ps2_device_id_t device_id;
} ps2_addr_t;

int ps2_read(void);
int ps2_send(uint8_t port,uint8_t data);

#endif
