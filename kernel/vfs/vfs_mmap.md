---
title: vfs_mmap
---
```c
int vfs_mmap(vfs_fd_t *fd, off_t offset, vmm_seg_t *seg);
```

## use
`vfs_mmap` call the file/device specific `mmap` operation to memory map the file/device in memory on the specified seg.
