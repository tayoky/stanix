#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define VFS_MAX_NODE_NAME_LEN 256

struct vfs_node_struct;

typedef struct vfs_node_struct {
	void *private_inode;
	uint64_t ref_count;
	uint64_t (* read)(struct vfs_node_struct *,void *buf,uint64_t off,size_t count);
	uint64_t (* write)(struct vfs_node_struct *,void *buf,uint64_t off,size_t count);
	uint64_t (* close)(struct vfs_node_struct *);
	struct vfs_node_struct (* finddir)(struct vfs_node_struct *,char *name);
}vfs_node;


struct kernel_table_struct;
void init_vfs(struct kernel_table_struct *kernel);

int vfs_mount(struct kernel_table_struct *kernel,const char *path,vfs_node *mounting_node);


uint64_t vfs_read(vfs_node *node,const void *buffer,uint64_t offset,size_t count);
uint64_t vfs_write(vfs_node *node,void *buffer,uint64_t offset,size_t count);
void vfs_close(kernel_table *kernel,vfs_node *node);
#endif