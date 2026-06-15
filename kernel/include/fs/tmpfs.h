#ifndef KERNEL_TMPFS_H
#define KERNEL_TMPFS_H

#include <kernel/cache.h>
#include <kernel/list.h>
#include <kernel/vfs.h>
#include <kernel/vmm.h>
#include <sys/time.h>
#include <sys/types.h>
#include <limits.h>

void init_tmpfs();
vfs_superblock_t *new_tmpfs(void);

struct tmpfs_inode;

typedef struct tmpfs_inode {
	vfs_node_t vnode;
	struct tmpfs_inode *parent;
	size_t link_count;
	union {
		struct {
			cache_t cache;
		} file;
		struct {
			void *buffer;
			size_t buffer_size;
		} link;
		struct {
			list_t entries;
		} directory;
		dev_t dev;
	};
} tmpfs_inode_t;

typedef struct tmpfs_dirent {
	list_node_t node;
	char name[PATH_MAX];
	tmpfs_inode_t *inode;
} tmpfs_dirent_t;

#endif
