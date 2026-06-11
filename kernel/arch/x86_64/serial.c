#include <kernel/port.h>
#include <kernel/serial.h>
#include <kernel/earlycon.h>
#include <stdint.h>

static void serial_write_char(const char data) {
	while (!(in_byte(SERIAL_PORT + 5) & 0x20));
	out_byte(SERIAL_PORT, data);
}


static ssize_t serial_output(earlycon_t *earlycon, const char *buf, size_t count) {
	(void)earlycon;
	for (size_t i=0; i < count; i++) {
		serial_write_char(buf[i]);
	}
	return count;
}

static earlycon_t serial_con = {
	.name = "serial",
	.output = serial_output,
};

int init_serial(void) {
	out_byte(SERIAL_PORT + 1, 0x00);
	out_byte(SERIAL_PORT + 3, 0x80);
	out_byte(SERIAL_PORT + 0, 0x03);
	out_byte(SERIAL_PORT + 1, 0x00);
	out_byte(SERIAL_PORT + 3, 0x03);
	out_byte(SERIAL_PORT + 2, 0xC7);
	out_byte(SERIAL_PORT + 4, 0x0B);
	out_byte(SERIAL_PORT + 4, 0x1E);
	out_byte(SERIAL_PORT + 0, 0xAE);
	if (in_byte(SERIAL_PORT + 0) != 0xAE) {
		return 1;
	}
	out_byte(SERIAL_PORT + 4, 0x0F);
	earlycon_register(&serial_con);
	return 0;
}
