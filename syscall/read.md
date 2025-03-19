# read
```c
ssize_t read(int fd,void *buffer,size_t count);
```
## use
read `count` bytes from `fd` to `buffer`
## return code
`read` return the number of bytes read or -1 and set `errno` if an error happend  
- `EFAULT`
  `buffer` point to outside of the addressable space
- `EBADF`
  the specified file descriptor don't exist , is not open or is not open for reading
- `EISDIR`
  the specified file descriptor is a directory  
  NOTE : use the [`readdir` syscall](readdir.md) or even better the [opendir api]()
- `EIO`
  io error
