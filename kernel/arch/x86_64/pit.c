#include <kernel/arch.h>
#include <kernel/interrupt.h>
#include <kernel/irq.h>
#include <kernel/kernel.h>
#include <kernel/pit.h>
#include <kernel/port.h>
#include <kernel/print.h>
#include <kernel/serial.h>
#include <kernel/time.h>

#define PIT_CHANNEL0 0x40
#define PIT_CHANNEL1 0x41
#define PIT_CHANNEL2 0x42
#define PIT_COMMAND  0x43
#define TPS          100

static struct timespec time;

int gettime(clockid_t clock, struct timespec *ts) {
	(void)clock;
	*ts = time;
	return 0;
}

int settime(clockid_t clock, struct timespec *ts) {
	(void)clock;
	time = *ts;
	return 0;
}

void pit_handler(registers_t *registers, void *data) {
	(void)data;

	// update the time
	time.tv_nsec += 1000000000 / TPS;
	if (time.tv_nsec >= 1000000000) {
		time.tv_nsec -= 1000000000;
		time.tv_sec++;
	}

	timer_handler(registers);
}

void init_pit(void) {
	kstatusf("init PIT... ");

	// the tick per second is defined here
	uint16_t divider = 1193181 / TPS;
	irq_register_handler(irq_hirq2irq(0), pit_handler, NULL);

	out_byte(PIT_COMMAND, 0b00110100);
	out_byte(PIT_CHANNEL0, divider & 0xFF);
	out_byte(PIT_CHANNEL0, (divider >> 8) & 0xFF);

	kok();
}
