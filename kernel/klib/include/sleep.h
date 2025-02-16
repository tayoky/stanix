#ifndef SLEEP_H
#define SLEEP_H

#include <sys/time.h>

void sleep_until(struct timeval wakeup_time);
void sleep(long second);
void micro_sleep(suseconds_t micro_second);

#endif