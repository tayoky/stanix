# vfs
## base
the vfs as multplie root, one for each mount point  
each `vfs_node` represent an context
## path
path are like `mountpoint:/folder/file.txt`
## function
### `vfs_mount(const char *path,vfs_node *node)`
mount a node at a specifed place   
NOTE : a mounted context cannot be close, `vfs_close`on it do nothing  
#### return value
return 0 on succes else error code
#### ex : 
```c
vfs_node *my_node = new_tmpfs();
vfs_mount("test",my_node);
```

### `vfs_close(vfs_node *node)`
close an context 
#### ex
```c
vfs_node *file = vfs_open("initrd:/readme.txt");
vfs_close(file);
```