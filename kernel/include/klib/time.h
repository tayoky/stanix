#ifndef KERNEL_TIME_H
#define KERNEL_TIME_H

#include <sys/time.h>

/**
 * @brief convert date to time_t, used a lot for DOS stuff
 */
time_t date2time(long year, long month, long day, long hour, long minute, long second);

void time2date(time_t time, long *year, long *month, long *day, long *hour, long *minute, long *second);

/**
 * @brief comapre two timespec
 */
int timespec_cmp(struct timespec *time1, struct timespec *time2);

#define NSEC_PER_SEC 1000000000

static void timespec_add(struct timespec *dest, struct timespec a, struct timespec *b) {
	dest->tv_sec  = a->tv_sec  + b->tv_sec;
	dest->tv_nsec = a->tv_nsec + b->tv_nsec;
	if (dest->tv_nsec > NSEC_PER_SEC) {
		dest->tv_sec++;
		dest->tv_nsec %= NSEC_PER_SEC;
	}
	return a;
}

static void timespec_sub(struct timespec *dest, struct timespec a, struct timespec *b) {
	dest->tv_sec  = a->tv_sec  - b->tv_sec;
	dest->tv_nsec = a->tv_nsec - b->tv_nsec;
	if (dest->tv_nsec < 0) {
		dest->tv_sec--;
		dest->tv_nsec += NSEC_PER_SEC;
	}
	return a;
}

int gettime(clockid_t clock, struct timespec *time);
int settime(clockid_t clock, struct timespec *time);


static inline int gettime_sec(clockid_t clock) {
    struct timespec time;
    gettime(clock, &time);
    return time.tv_sec;
}

#endif
