# write
```c
ssize_t write(int fd,void *buffer,size_t count);
```
## use
write `count` bytes from `buffer` to `fd`
## return code
`write` return the number of bytes written or -1 and set `errno` if an error happend  
- `EFAULT`
  `buffer` point to outside of the addressable space
- `EBADF`
  the specified file descriptor don't exist , is not open or is not open for wrting
- `EISDIR`
  the specified file descriptor is a directory  
  NOTE : this error is normally impossible because the [`open` syscall](open.md) can't open directory for wruting
- `EIO`
  io error
