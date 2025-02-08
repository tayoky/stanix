#ifndef SYS_H
#define SYS_H

#include <stdint.h>
#include "scheduler.h"

void sys_exit(uint64_t error_code);
pid_t sys_getpid();
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

#endif