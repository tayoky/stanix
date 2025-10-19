---
title : usleep
---

## WARNING
this syscall have been removed from stanix's kernel and is replaced by [nanosleep](nanosleep)

```c
int usleep(useconds_t usec);
```

## use
stop the process for (at least ) `usec` microsecond  
note that the process might be stoped for a bit longer than `usec` because of the time to process syscall

## return value
return 0 on success or return -1 and set errno on error

- `EINTR`  
  `usleep` was interrutped by a signal
