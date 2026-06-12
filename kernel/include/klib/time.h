#ifndef KERNEL_TIME_H
#define KERNEL_TIME_H

#include <sys/time.h>

/**
 * @brief convert date to time_t, used a lot for DOS stuff
 */
time_t date2time(long year, long month, long day, long hour, long minute, long second);

/**
 * @brief comapre two timespec
 */
int timespec_cmp(struct timespec *time1, struct timespec *time2);

int gettime(clockid_t clock, struct timespec *time);
int settime(clockid_t clock, struct timespec *time);


static inline gettime_sec(clockid_t clock) {
    struct timespec time;
    gettime(clock, &time);
    return time.tv_sec;
}

#endif
