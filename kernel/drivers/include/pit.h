#ifndef PIT_H
#define PIT_H

#include <stdint.h>

void init_pit(void);
void pit_sleep(uint64_t tick);

#endif