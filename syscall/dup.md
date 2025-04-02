# dup
```c
int dup(int fd);
```
## use
duplicate a file descriptor
## return code
return the copy of the file descriptor on sucess or return -1 and set errno on error
- `EBADF`  
  `fd` is not open or don't exist
- `EIO`  
  an IO error happend while duplicating `fd`
