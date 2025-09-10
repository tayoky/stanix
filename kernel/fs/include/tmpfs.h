#ifndef TMPFS_H
#define TMPFS_H


#include <kernel/list.h>
#include <kernel/vfs.h>
#include <sys/types.h>
#include <sys/time.h>
#include <limits.h>

void init_tmpfs();
vfs_node *new_tmpfs();

struct tmpfs_inode_struct;

typedef struct tmpfs_inode_struct{
	struct tmpfs_inode_struct *parent;
	list *entries;
	uint64_t flags;
	size_t buffer_size;
	char *buffer;
	mode_t perm;
	uid_t owner;
	gid_t group_owner;
	time_t atime;
	time_t ctime;
	time_t mtime;
	size_t link_count;
	size_t open_count;
}tmpfs_inode;

typedef struct tmpfs_dirent_struct {
	char name[PATH_MAX];
	tmpfs_inode *inode;
} tmpfs_dirent;

vfs_node *tmpfs_lookup(vfs_node *node,const char *name);
ssize_t tmpfs_read(vfs_node *node,void *buffer,uint64_t offset,size_t count);
ssize_t tmpfs_write(vfs_node *node,void *buffer,uint64_t offset,size_t count);
void tmpfs_close(vfs_node *node);
int tmpfs_create(vfs_node *node,const char *name,mode_t perm,long flags);
int tmpfs_unlink(vfs_node *node,const char *name);
int tmpfs_link(vfs_node *,const char*,vfs_node*,const char*);
struct dirent *tmpfs_readdir(vfs_node *node,uint64_t index);
int tmpfs_truncate(vfs_node *node,size_t size);
int tmpfs_symlink(vfs_node *node,const char *name,const char *target);
ssize_t tmpfs_readlink(vfs_node *node,char *buf,size_t bufsize);
int tmpfs_getattr(vfs_node *node,struct stat *st);
int tmpfs_setattr(vfs_node *node,struct stat *st);

#define TMPFS_FLAGS_FILE 0x01
#define TMPFS_FLAGS_DIR  0x02
#define TMPFS_FLAGS_LINK 0x04

#define IOCTL_TMPFS_CREATE_DEV 0x01
#define IOCTL_TMPFS_SET_DEV_INODE 0x02

#endif