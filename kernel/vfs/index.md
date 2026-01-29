---
title: kernel vfs
---
The **vfs (virtual filesystem)** provide filesystem abstraction and allows opening devices as files.

## types
- [`vfs_fd_t`](vfs_fd_t)
- [`vfs_node_t`](vfs_node_t)

## functions

### files/devices operations
- [`vfs_open`](vfs_open)
- [`vfs_openat`](vfs_openat)
- [`vfs_open_node`](vfs_open_node)
- [`vfs_dup`](vfs_dup)
- [`vfs_close`](vfs_close)
- [`vfs_read`](vfs_read)
- [`vfs_write`](vfs_write)
- [`vfs_ioctl`](vfs_ioctl)
- [`vfs_mmap`](vfs_mmap)
- [`vfs_truncate`](vfs_truncate)

### inodes operations
- [`vfs_open_node`](vfs_open_node)
- [`vfs_get_node_at`](vfs_get_node_at)
- [`vfs_get_node`](vfs_get_node)
- [`vfs_dup_node`](vfs_dup_node)
- [`vfs_close_node`](vfs_close_node)
- [`vfs_getattr`](vfs_getattr)
- [`vfs_setattr`](vfs_setattr)
- [`vfs_chmod`](vfs_chmod)
- [`vfs_chown`](vfs_chown)
- [`vfs_perm`](vfs_perm)
- [`vfs_user_perm`](vfs_user_perm)
- [`vfs_link`](vfs_link)
- [`vfs_unlink`](vfs_unlink)
- [`vfs_symlink`](vfs_symlink)
- [`vfs_readlink`](vfs_readlink)
- [`vfs_lookup`](vfs_lookup)

### vfs operations
- [`vfs_register_fs`](vfs_register_fs)
- [`vfs_unregister_fs`](vfs_unregister_fs)
