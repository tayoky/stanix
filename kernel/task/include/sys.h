#ifndef SYS_H
#define SYS_H

#include <stdint.h>
#include <kernel/arch.h>
#include <kernel/scheduler.h>

void init_syscall(void);

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

void syscall_handler(fault_frame *context);
int sys_open(const char *, int, mode_t);
int sys_close(int);
ssize_t sys_write(int, void *, size_t);
ssize_t sys_read(int, void *, size_t);
void sys_exit(int);
int sys_dup(int);
int sys_dup2(int, int);
off_t sys_seek(int, int64_t, int);
int sys_ioctl(int fd,uint64_t request,void *arg);
int sys_usleep(useconds_t usec);
int sys_sleepuntil(struct timeval *time);
int sys_gettimeofday(struct timeval *tv, struct timezone *tz);
uint64_t sys_sbrk(intptr_t incr);
pid_t sys_getpid();

#endif