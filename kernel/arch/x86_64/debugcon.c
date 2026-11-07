#include <kernel/earlycon.h>
#include <kernel/serial.h>
#include <kernel/port.h>

// simple driver for QEMU/BOCHS debugcon

#define DEBUGCON_PORT 0xe9

static void debugcon_write_char(char c) {
	out_byte(DEBUGCON_PORT, c);
}

static ssize_t debugcon_output(earlycon_t *earlycon, const char *buf, size_t count) {
	(void)earlycon;
	for (size_t i=0; i < count; i++) {
		debugcon_write_char(buf[i]);
	}
	return count;
}

static earlycon_t debugcon_con = {
	.name = "debugcon",
	.output = debugcon_output,
};

int init_debugcon(void) {
	// check if the debugcon is present
	uint8_t readback = in_byte(DEBUGCON_PORT);
	if (readback == 0 || readback == 0xff) {
		return -1;
	}
	earlycon_register(&debugcon_con);
	return 0;
}
