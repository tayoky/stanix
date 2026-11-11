#include <kernel/mutex.h>
#include <kernel/print.h>
#include <kernel/arch.h>
#include <kernel/rtc.h>
#include <stdint.h>

#define CMOS_ADDRESS_PORT 0x70
#define CMOS_DATA_PORT    0x71

#define CMOS_REG_SECOND   0x00
#define CMOS_REG_MINUTE   0x02
#define CMOS_REG_HOUR     0x04
#define CMOS_REG_DAY      0x07
#define CMOS_REG_MONTH    0x08
#define CMOS_REG_YEAR     0x09
#define CMOS_REG_STATUS_A 0x0A
#define CMOS_REG_STATUS_B 0x0B

#define CMOS_STATUS_BIT    0x80
#define CMOS_STATUS_24HOUR 0x02
#define CMOS_STATUS_BINARY 0x04

static uint8_t cmos_mode;
static mutex_t cmos_lock;
static rtc_device_t *cmos_device;

static uint8_t cmos_read_register(uint8_t reg) {
	out_byte(CMOS_ADDRESS_PORT, reg);
	return in_byte(CMOS_DATA_PORT);
}

static void cmos_write_register(uint8_t reg, uint8_t data) {
	out_byte(CMOS_ADDRESS_PORT, reg);
	out_byte(CMOS_DATA_PORT, data);
}

void cmos_wait(void) {
	// wait until update finish
	while (cmos_read_register(CMOS_REG_STATUS_A) & CMOS_STATUS_BIT);
}

uint8_t cmos_read(uint8_t reg) {
	uint8_t data = cmos_read_register(reg);
	if (!(cmos_mode & CMOS_STATUS_BINARY)) {
		// BCD to binary conversion
		data = ((data >> 4) * 10) + (data & 0x0f);
	}
	return data;
}

void cmos_write(uint8_t reg, uint8_t data) {
	if (!(cmos_mode & CMOS_STATUS_BINARY)) {
		// binary to BCD conversion
		data = ((data / 10) << 4) + (data % 10);
	}
	cmos_write_register(reg, data);
}

static int cmos_get_time(rtc_device_t *rtc_device, struct timespec *time) {
	(void)rtc_device;
	mutex_acquire(&cmos_lock);

	// we need to read the same values at least twice 
	// in a row to make sure we didn't read during 
	// an update and got fucked up values
	uint8_t second, minute, hour, day, month;
	long year;
	int got_correct_values = 0;

	for (int i = 0; i < 10; i++) {
		cmos_wait();
		uint8_t old_second = cmos_read(CMOS_REG_SECOND);
		uint8_t old_minute = cmos_read(CMOS_REG_MINUTE);
		uint8_t old_hour   = cmos_read(CMOS_REG_HOUR);
		uint8_t old_day    = cmos_read(CMOS_REG_DAY);
		uint8_t old_month  = cmos_read(CMOS_REG_MONTH);
		uint8_t old_year   = cmos_read(CMOS_REG_YEAR);

		cmos_wait();
		second = cmos_read(CMOS_REG_SECOND);
		minute = cmos_read(CMOS_REG_MINUTE);
		hour   = cmos_read(CMOS_REG_HOUR);
		day    = cmos_read(CMOS_REG_DAY);
		month  = cmos_read(CMOS_REG_MONTH);
		year   = cmos_read(CMOS_REG_YEAR);

		if (old_second != second) continue;
		if (old_minute != minute) continue;
		if (old_hour   != hour) continue;
		if (old_day    != day) continue;
		if (old_month  != month) continue;
		if (old_year   != year) continue;
		got_correct_values = 1;
		break;
	}

	if (!got_correct_values) {
		// we got stuck in a  loop
		kwarningf("timed out");
		mutex_release(&cmos_lock);
		return -ETIMEDOUT;
	}

	// fun fact, stanix was created in 2024, so it cannot be used before 2010
	// this code will work until 2110, and then it won't be my problem anymore :D
	if (year > 10) {
		year += 2000;
	} else {
		year += 2100;
	}

	time->tv_sec  = date2time(year, month, day, hour, minute, second);
	time->tv_nsec = 0;

	mutex_release(&cmos_lock);
	return 0;
}

static int cmos_set_time(rtc_device_t *rtc_device, const struct timespec *time) {
	(void)rtc_device;
	mutex_acquire(&cmos_lock);

	long year, month, day, hour, minute, second;
	time2date(time->tv_sec, &year, &month, &day, &hour, &minute, &second);

	cmos_wait();
	cmos_write(CMOS_REG_SECOND, second);
	cmos_write(CMOS_REG_MINUTE, minute);
	cmos_write(CMOS_REG_HOUR  , hour);
	cmos_write(CMOS_REG_DAY   , day);
	cmos_write(CMOS_REG_MONTH , month);
	cmos_write(CMOS_REG_YEAR  , year % 100);

	mutex_release(&cmos_lock);
	return 0;
}

static rtc_ops_t cmos_ops = {
	.get_time = cmos_get_time,
	.set_time = cmos_set_time,
};

void init_cmos(void) {
	kstatusf("start reading cmos... ");
	cmos_mode = cmos_read_register(CMOS_REG_STATUS_B);
	cmos_device = rtc_device_allocate();
	cmos_device->ops      = &cmos_ops;
	cmos_device->priority = 1;

	int cmos_major = device_allocate_major();
	int ret = rtc_device_register(cmos_device, "cmos", makedev(cmos_major, 0));
	kassert(ret == 0);

	// TODO : remove this when the clock subsystem is finished
	struct timespec time;
	rtc_get_time(&time);
	settime(CLOCK_REALTIME, &time);
	kok();

	kinfof("current time : %ld\n", time.tv_sec);
}
