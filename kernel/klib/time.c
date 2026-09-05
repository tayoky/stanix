
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

static long nbofdayin(long year, int month) {
	switch (month) {
	case 12:
		return 31;
	case 11:
		return 30;
	case 10:
		return 31;
	case 9:
		return 30;
	case 8:
		return 31;
	case 7:
		return 31;
	case 6:
		return 30;
	case 5:
		return 31;
	case 4:
		return 30;
	case 3:
		return 31;
	case 2:
		if (is_leap(year)) {
			return 29;
		} else {
			return 28;
		}
	case 1:
		return 31;

	default:
		return -1;
	}
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

void time2date(time_t time, long *_year, long *_month, long *_day, long *_hour, long *_minute, long *_second) {
	if (_second) *_second = time % 60;
	if (_minute) *_minute = (time / 60) % 60;
	if (_hour)   *_hour   = (time / 3600) % 24;

	long day = time / 86400;
	long year = 1970;
	for (;;) {
		if (is_leap(year)) {
			if (day < 366) {
				break;
			}
			day -= 366;
		} else {
			if (day < 365) {
				break;
			}
			day -= 365;
		}
		year++;
	}

	int month = 1;
	for (;;) {
		if (day < nbofdayin(year, month)) {
			break;
		}
		day -= nbofdayin(year, month);
		month++;
	}

	if (_day)   *_day   = day;
	if (_month) *_month = month;
	if (_year)  *_year  = year;
}

int timespec_cmp(struct timespec *time1, struct timespec *time2) {
	if (time1->tv_sec > time2->tv_sec) return 1;
	if (time1->tv_sec < time2->tv_sec) return -1;
	if (time1->tv_nsec > time2->tv_nsec) return 1;
	if (time1->tv_nsec < time2->tv_nsec) return -1;
	return 0;
}
