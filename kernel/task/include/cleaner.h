#ifndef CLEANER_H
#define CLEANER_H

#include <kernel/scheduler.h>

void cleaner_task();
void free_proc(process *proc);

#endif