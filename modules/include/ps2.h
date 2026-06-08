#ifndef PS2_H
#define PS2_H

#include <kernel/bus.h>
#include <stdint.h>

// devices commands
#define PS2_ECHO             0xEE
#define PS2_IDENTIFY         0xF2
#define PS2_ENABLE_SCANNING  0xF4
#define PS2_DISABLE_SCANNING 0xF5
#define PS2_RESET            0xFF

#define PS2_ACK               0xFA
#define PS2_RESEND            0xFE
#define PS2_SELF_TEST_PASSED  0xAA
#define PS2_SELF_TEST_FAILED1 0xFC
#define PS2_SELF_TEST_FAILED2 0xFD

typedef int ps2_device_id_t[2];

typedef struct ps2_addr {
	bus_addr_t addr;
	uint8_t port;
	ps2_device_id_t device_id;
} ps2_addr_t;

int ps2_read(void);
int ps2_send(uint8_t port, uint8_t data);
int ps2_reset(uint8_t port);

#endif
