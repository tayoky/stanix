#ifndef _KERNEL_TIME_H
#define _KERNEL_TIME_H

#include <sys/time.h>

/**
 * @brief convert date to time_t, used a lot for DOS stuff
 */
time_t date2time(long year, long month, long day, long hour, long minute, long second);

/**
 * @brief comapre two timeval
 */
int timeval_cmp(struct timeval *time1, struct timeval *time2);


extern struct timeval time;


#define NOW() time.tv_sec

#endif
