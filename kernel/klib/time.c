
#include <kernel/time.h>

static int is_leap(long year) {
	// not divisible by 4
	if (year % 4) {
		return 0;
	}

	// exception for mutiple of 100 that can't be divided by 400
	if ((year % 400) && !(year % 100)) {
		return 0;
	}

	return 1;
}

time_t date2time(long year, long month, long day, long hour, long minute, long second) {
	long days = day - 1;

	switch (month) {
	case 12:
		days += 30;
		// fallthrough
	case 11:
		days += 31;
		// fallthrough
	case 10:
		days += 30;
		// fallthrough
	case 9:
		days += 31;
		// fallthrough
	case 8:
		days += 31;
		// fallthrough
	case 7:
		days += 30;
		// fallthrough
	case 6:
		days += 31;
		// fallthrough
	case 5:
		days += 30;
		// fallthrough
	case 4:
		days += 31;
		// fallthrough
	case 3:
		if (is_leap(year)) {
			days += 29;
		} else {
			days += 28;
		}
		// fallthrough
	case 2:
		days += 31;
		// fallthrough
	case 1:
		break;
	}

	while (year > 1970) {
		year--;
		if (is_leap(year)) {
			days += 366;
		} else {
			days += 365;
		}
	}
	time_t time = second + minute * 60 + hour * 3600 + days * 86400;
	return time;
}