#ifndef TMPFS_H
#define TMPFS_H


#include <kernel/cache.h>
#include <kernel/list.h>
#include <kernel/vfs.h>
#include <kernel/vmm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <limits.h>

void init_tmpfs();
vfs_superblock_t *new_tmpfs(void);

struct tmpfs_inode;

typedef struct tmpfs_inode{
	list_node_t node;
	cache_t cache;
	struct tmpfs_inode *parent;
	void *buffer;
	size_t buffer_size;
	list_t entries;
	long type;
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
} tmpfs_inode_t;

typedef struct tmpfs_dirent {
	list_node_t node;
	char name[PATH_MAX];
	tmpfs_inode_t *inode;
} tmpfs_dirent_t;

#define TMPFS_TYPE_FILE  0x01
#define TMPFS_TYPE_DIR   0x02
#define TMPFS_TYPE_LINK  0x04
#define TMPFS_TYPE_SOCK  0x08
#define TMPFS_TYPE_CHAR  0x10
#define TMPFS_TYPE_BLOCK 0x20

#endif