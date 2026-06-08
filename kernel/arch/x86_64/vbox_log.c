#include <kernel/serial.h>
#include <kernel/earlycon.h>
#include <kernel/port.h>

// simple vbox logger

static void vbox_write_char(char c) {
	out_byte(0x504, c);
}

static ssize_t vbox_output(earlycon_t *earlycon, const char *buf, size_t count) {
	(void)earlycon;
	for (size_t i=0; i < count; i++) {
		vbox_write_char(buf[i]);
	}
	return count;
}

static earlycon_t vbox_con = {
	.name = "vbox",
	.output = vbox_output,
};

void init_vbox_log(void) {
	earlycon_register(&vbox_con);
}