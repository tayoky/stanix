#ifndef CLEANER_H
#define CLEANER_H

#include "scheduler.h"

void cleaner_task();
void free_proc(process *proc);

#endif