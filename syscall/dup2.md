# dup2
```c
int dup2(int oldfd,newfd);
```
## use
duplicate the file descritpor `oldfd` to `newfd`
## return code
return the copy of the file descriptor on sucess or return -1 and set errno on error
- `EBADF`  
  `oldfd` is not open or don't exist
- `EBADF`  
  `newfd` is aready open or don't exist
- `EIO`  
  an IO error happend while duplicating `fd`
