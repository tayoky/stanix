# close
```c
int close(int fd);
```
## use
close the specified file descriptor
## return code
return 0 on sucess or -1 and set errno on error  
- `EBADF`  
  `fd`don't exist or is not open

