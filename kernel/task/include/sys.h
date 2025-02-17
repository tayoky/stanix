#ifndef SYS_H
#define SYS_H

#include <stdint.h>
#include "scheduler.h"

void init_syscall(void);

#define O_RDONLY	00000000
#define O_WRONLY	00000001
#define O_RDWR		00000002
#ifndef O_CREAT
#define O_CREAT		00000100	
#endif
#ifndef O_EXCL
#define O_EXCL		00000200	
#endif
#ifndef O_NOCTTY
#define O_NOCTTY	00000400	
#endif
#ifndef O_TRUNC
#define O_TRUNC		00001000	
#endif
#ifndef O_APPEND
#define O_APPEND	00002000
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY	00200000	/* must be a directory */
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW	00400000	/* don't follow links */
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC	02000000	/* set close_on_exec */
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

void init_syscall(void);
static int find_fd();
int sys_open(const char *, int, mode_t);
int sys_close(int);
int64_t sys_write(int, void *, size_t);
int64_t sys_read(int, void *, size_t);
void sys_exit(uint64_t);
int sys_dup(int);
int sys_dup2(int, int);
int64_t sys_seek(int, int64_t, int);
int sys_ioctl(int fd,uint64_t request,void *arg);
int sys_usleep(useconds_t usec);
int sys_sleepuntil(struct timeval *time);
int sys_gettimeofday(struct timeval *tv, struct timezone *tz);
pid_t sys_getpid();

#endif