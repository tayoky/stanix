#ifndef KERNEL_FUTEX_H
#define KERNEL_FUTEX_H

int do_futex(long *addr, int op, long val);
void init_futexes(void);

#endif
