#ifndef _KERNEL_TIME_H
#define _KERNEL_TIME_H

#include <sys/time.h>

/**
 * @brief convert date to time_t, used a lot for DOS stuff
 */
time_t date2time(long year, long month, long day, long hour, long minute, long second);

extern struct timeval time;

int sleep_until(struct timeval wakeup_time);
int sleep(long second);
int micro_sleep(suseconds_t micro_second);

#define NOW() time.tv_sec

#endif
