#include <kernel/vfs.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/device.h>
#include <kernel/sysfs.h>
#include <kernel/print.h>
#include <kernel/slab.h>
#include <kernel/pmm.h>
#include <kernel/bus.h>
#include <errno.h>

static slab_cache_t sysfs_inodes_cache;
static vfs_inode_ops_t sysfs_inode_ops;

#define INODE_ROOT   1
#define INODE_BLOCK  2
#define INODE_CHAR   3
#define INODE_BUS    4
#define INODE_KERNEL 5

static char *root_entries[] = {
	"..",
	".",
	[INODE_BLOCK]  = "block",
	[INODE_CHAR]   = "char",
	[INODE_BUS]    = "bus",
	[INODE_KERNEL] = "kernel",
};

static sysfs_inode_t *sysfs_new_inode(int type, void *ptr) {
	sysfs_inode_t *inode = slab_alloc(&sysfs_inodes_cache);
	if (!inode) return NULL;
	memset(inode, 0, sizeof(sysfs_inode_t));
	inode->node.ops = &sysfs_inode_ops;
	inode->type = type;
	inode->ptr  = ptr;
	if (ptr) {
		inode->node.flags = VFS_FILE;
	} else {
		inode->node.flags = VFS_DIR;
	}
	return inode;
}

static int sysfs_lookup(vfs_node_t *vnode, vfs_dentry_t *dentry) {
	sysfs_inode_t *inode = container_of(vnode, sysfs_inode_t, node);
	sysfs_inode_t *child_inode = NULL;
	switch (inode->type) {
	case INODE_ROOT:
		for (size_t i=0; i<arraylen(root_entries); i++) {
			if (!strcmp(root_entries[i], dentry->name)) {
				child_inode = sysfs_new_inode(i, NULL);
			}
		}
		break;
	}
	if (child_inode) {
		child_inode->node.superblock = vnode->superblock;
		dentry->inode = &child_inode->node;
		return 0;
	}
	return -ENOENT;
}

static int sysfs_readdir(vfs_node_t *vnode, unsigned long index, struct dirent *dirent) {
	sysfs_inode_t *inode = container_of(vnode, sysfs_inode_t, node);
	if (index < 2) {
		strcpy(dirent->d_name, root_entries[index]);
		dirent->d_ino = index;
		dirent->d_type = DT_DIR;
		return 0;
	}
	switch (inode->type) {
	case INODE_ROOT:;
		if (index >= arraylen(root_entries)) return -ENOENT;
		strcpy(dirent->d_name, root_entries[index]);
		dirent->d_ino = index;
		dirent->d_type = DT_DIR;
		return 0;
	case INODE_BUS:
		index -= 2;
		hashmap_foreach(key, element, &devices) {
			(void)key;
			bus_t *bus = (bus_t*)element;
			if (bus->device.type != DEVICE_BUS) continue;
			if (index == 0) {
				strcpy(dirent->d_name, bus->device.name);
				dirent->d_type = DT_DIR;
			}
			index--;
		}
		return -ENOENT;
	default:
		return -ENOSYS;
	}
}

static int sysfs_getattr(vfs_node_t *vnode, struct stat *stat) {
	sysfs_inode_t *inode = container_of(vnode, sysfs_inode_t, node);

	// allow execute perm only on directories
	if (inode->ptr) {
		stat->st_mode = 0444;
	} else {
		stat->st_mode = 0555;
	}

	return 0;
}

static void sysfs_cleanup(vfs_node_t *vnode) {
	sysfs_inode_t *inode = container_of(vnode, sysfs_inode_t, node);
	slab_free(inode);
}

static vfs_inode_ops_t sysfs_inode_ops = {
	.lookup  = sysfs_lookup,
	.readdir = sysfs_readdir,
	.getattr = sysfs_getattr,
	.cleanup = sysfs_cleanup,
};

static int sysfs_mount(const char *source, const char *target, unsigned long flags, const void *data, vfs_superblock_t **out_superblock) {
	(void)source;
	(void)target;
	(void)flags;
	(void)data;

	vfs_superblock_t *superblock = kmalloc(sizeof(vfs_superblock_t));
	memset(superblock, 0, sizeof(vfs_superblock_t));
	sysfs_inode_t *root = sysfs_new_inode(INODE_ROOT, NULL);
	root->node.ref_count = 1;
	superblock->root = &root->node;

	*out_superblock = superblock;
	return 0;
}

static vfs_filesystem_t sysfs_filesystem = {
	.name = "sysfs",
	.mount = sysfs_mount,
};

void init_sysfs(void) {
	kstatusf("init sysfs ...");
	slab_init(&sysfs_inodes_cache, sizeof(sysfs_inode_t), "sysfs nodes");
	vfs_register_fs(&sysfs_filesystem);
	kok();
}