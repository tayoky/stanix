---
title: open
---
```c
int open(const char *path,int flags,... /*mode_t mode*/);
```
## use
`open` open a file descriptor to the specified pathname.  
The flags argument allow to open for specific operation and set flags on the file decsriptor.

## avalibles flags
- `O_READONLY`  
   open in readonly
- `O_WRITEONLY`
- `O_CREAT`  
  create the file is it don't exist the optional argument `mode` is used with this flags
- `O_EXCL`  
  when used with `O_CREAT` , make sure a file is created trigger an error if it can't
- `O_CLOEXEC`  
  close the file descriptor when the [`execve` syscall](execve.md) is call
- `O_APPEND`  
  set the file desciptor in append mode , before each write , the kernel seek to the end of the file
- `O_DIRECTORY`  
  open a directory  
  if you want to open a directory you might want to use [`opendir`]() instead of the raw syscall
## return code
open return the id the the new file descriptor or -1 and set `errno` if an error happend
- `EFAULT`  
  `path` point to outside of the addrassable space
- `EEXIST`  
  `O_CREAT` and `O_EXCL` were set but the file aready exist
- `ENOENT`  
  `path` don't exist
- `EISDIR`  
  `path` is a directory but `O_DIRECTORY` was noe specified
- `ENOTDIR`  
  `O_DIRECTORY` was specified but `path` is not a directory
- `EPERM`  
  permission issue
