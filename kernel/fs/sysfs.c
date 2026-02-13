#include <kernel/vfs.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/sysfs.h>
#include <kernel/print.h>
#include <kernel/pmm.h>
#include <errno.h>

#define INODE_ROOT   1
#define INODE_BLOCK  2
#define INODE_CHAR   3
#define INODE_BUS    4
#define INODE_KERNEL 5


static int sysfs_lookup(vfs_node_t *node, vfs_dentry_t *dentry) {
	
}

static int sysfs_readdir(vfs_node_t *node, unsigned long index, struct dirent *dirent) {
	
}


void init_sysfs(void) {
	kstatusf("init sysfs ...");
	kok();
}