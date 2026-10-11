#ifndef KERNEL_COND_VAR_H
#define KERNEL_COND_VAR_H

#include <kernel/atomic.h>

typedef ATOMIC(unsigned int) cond_var_t;

int cond_var_wait(cond_var_t *var, unsigned int mask, unsigned int value);
int cond_var_wait_interruptible(cond_var_t *var, unsigned int mask, unsigned int value);
void cond_var_wakeup(cond_var_t *var);

#endif
