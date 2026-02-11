---
title: kernel vfs
---
The **vfs (virtual filesystem)** provide filesystem abstraction and allows opening devices as files.
The prototype of most vfs functions is in `<kernel/kheap.h>`.

## dentry
A `vfs_dentry` represent a directory entry, a cached entry must be **positive** (linked to an **inode**) but non cached entry can be **negative** (not linked to an inode).
The full path of a dentry can be obtained using `vfs_dentry_path`.
Dentries keep a reference to their inodes, but not the other way around, the dentries of an inode can be freed if not used.

## inodes
A `vfs_node_t` represent an inode. Various operations can be done using an inode.

### inodes functions
- `vfs_get_node_at`
- `vfs_get_node`
  Get the inode at a specified path.
- `vfs_dup_node`
  Increment ref count of an inode.
- `vfs_close_node`
  Decrement ref count of an inode and can destroy it if it reach zero (or keep it as cache).
- `vfs_getattr`
- `vfs_setattr`
  Get/set attribute of an inode.
- `vfs_chmod`
  Atomicly change mode of an inode.
- `vfs_chown`
  Atomicly change the owner of an inode.
- `vfs_perm`
- `vfs_user_perm`
  Get the permission of an inode.
- `vfs_link`
  Create a hardlink (NOTE : some filesystem does not support this).
- `vfs_unlink`
  Unlink a file.
- `vfs_symlink`
  Create a symlink.
- `vfs_readlink`
  Read the target of a symlink.
- `vfs_lookup`
  Lookup an entry on the inode of a directory.

## files context
To read or write a file or a device, a `vfs_fd_t` (file/device context) is required.
It is possible to get one using various functions such as `vfs_open` or `open_device`.

### files functions
- `vfs_open`
- `vfs_openat`
  Open a new context from a path.
- `vfs_open_node`
  Open a new context from an inode.
- `vfs_dup`
  Increment the ref count of a context.
- `vfs_close`
  Decrement the ref count of a context and close it if it reach zero.
- `vfs_read`
  Read from a file/device.
- `vfs_write`
  Write to the file/device.
- `vfs_ioctl`
  Do some device specific operation.
- `vfs_mmap`
  Memory map the context on a [`vmm_seg_t`](vmm)
- `vfs_truncate`
  Truncate the file/device.

### filesystems
**File systems** are represented by a `vfs_filesystem_t`.
A file system type can be registered using `vfs_register_fs` and unregistered using `vfs_unregister_fs`.
