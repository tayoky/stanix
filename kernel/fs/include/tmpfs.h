#ifndef TMPFS_H
#define TMPFS_H


#include <kernel/list.h>
#include <kernel/vfs.h>
#include <kernel/memseg.h>
#include <sys/types.h>
#include <sys/time.h>
#include <limits.h>

void init_tmpfs();
vfs_node_t *new_tmpfs();

struct tmpfs_inode_struct;

typedef struct tmpfs_inode_struct{
	struct tmpfs_inode_struct *parent;
	list_t *entries;
	uint64_t flags;
	size_t buffer_size;
	char *buffer;
	mode_t perm;
	uid_t owner;
	gid_t group_owner;
	time_t atime;
	time_t ctime;
	time_t mtime;
	dev_t dev;
	size_t link_count;
	size_t open_count;
	void *data;
}tmpfs_inode;

typedef struct tmpfs_dirent_struct {
	char name[PATH_MAX];
	tmpfs_inode *inode;
} tmpfs_dirent;

#define TMPFS_FLAGS_FILE  0x01
#define TMPFS_FLAGS_DIR   0x02
#define TMPFS_FLAGS_LINK  0x04
#define TMPFS_FLAGS_SOCK  0x08
#define TMPFS_FLAGS_CHAR  0x10
#define TMPFS_FLAGS_BLOCK 0x20

#endif