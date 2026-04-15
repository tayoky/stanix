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
static vfs_fd_ops_t sysfs_fd_ops;

#define INODE_ROOT       1
#define INODE_BLOCK_DIR  2
#define INODE_CHAR_DIR   3
#define INODE_BUS_DIR    4
#define INODE_BUS        5
#define INODE_BUS_ADDR   6
#define INODE_KERNEL_DIR 7
#define INODE_SLAB_DIR   8
#define INODE_SLAB       9
#define INODE_MEM        10

typedef struct static_entry {
	int type;
	int inode;
	const char *name;
} static_entry_t;

#define ENTRY(_type, _inode, _name)  {.type = _type, .inode = _inode, .name = _name}

static static_entry_t root_entries[] = {
	ENTRY(VFS_DIR, INODE_BLOCK_DIR , "block"),
	ENTRY(VFS_DIR, INODE_CHAR_DIR  , "char"),
	ENTRY(VFS_DIR, INODE_BUS_DIR   , "bus"),
	ENTRY(VFS_DIR, INODE_KERNEL_DIR, "kernel"),
	ENTRY(VFS_FILE, INODE_MEM      , "mem"),
};

static static_entry_t kernel_entries[] = {
	ENTRY(VFS_DIR, INODE_SLAB_DIR, "slab"),
};

static sysfs_inode_t *sysfs_new_inode(int type, void *ptr, int flags) {
	sysfs_inode_t *inode = slab_alloc(&sysfs_inodes_cache);
	if (!inode) return NULL;
	memset(inode, 0, sizeof(sysfs_inode_t));
	inode->node.ref_count = 1;
	inode->node.ops = &sysfs_inode_ops;
	inode->node.flags = flags;
	inode->type = type;
	inode->ptr  = ptr;
	return inode;
}

static int sysfs_static_entries_readdir(static_entry_t *entries, size_t entries_count, unsigned long index, struct dirent *dirent) {
	if (index >= entries_count) {
		return -ENOENT;
	}

	strcpy(dirent->d_name, entries[index].name);
	dirent->d_ino = entries[index].inode;
	switch (entries[index].type) {
	case VFS_DIR:
		dirent->d_type = DT_DIR;
		break;
	case VFS_FILE:
		dirent->d_type = DT_REG;
		break;
	default:
		dirent->d_type = DT_UNKNOWN;
		break;
	}

	return 0;
}

static sysfs_inode_t *sysfs_static_entries_lookup(static_entry_t *entries, size_t entries_count, const char *name) {
	for (size_t i=0; i < entries_count; i++) {
		if (!strcmp(entries[i].name, name)) {
			return sysfs_new_inode(entries[i].inode, NULL, entries[i].type);
		}
	}
	return NULL;
}

static int sysfs_lookup(vfs_node_t *vnode, vfs_dentry_t *dentry) {
	sysfs_inode_t *inode = container_of(vnode, sysfs_inode_t, node);
	sysfs_inode_t *child_inode = NULL;
	switch (inode->type) {
	case INODE_ROOT:
		child_inode = sysfs_static_entries_lookup(root_entries, arraylen(root_entries), dentry->name);
		break;
	case INODE_BUS_DIR:
		hashmap_foreach(key, element, &devices) {
			(void)key;
			bus_t *bus = container_of(element, bus_t, device);
			if (bus->device.type != DEVICE_BUS) continue;
			if (!strcmp(bus->device.name, dentry->name)) {
				child_inode = sysfs_new_inode(INODE_BUS, bus, VFS_DIR);
				break;;
			}
		}
		break;
	case INODE_BUS:;
		bus_t *bus = inode->ptr;
		foreach(node, &bus->addresses) {
			bus_addr_t *addr = container_of(node, bus_addr_t, node);
			if (!strcmp(addr->name, dentry->name)) {
				child_inode = sysfs_new_inode(INODE_BUS_ADDR, addr, VFS_DIR);
				break;
			}
		}
		break;
	case INODE_KERNEL_DIR:
		child_inode = sysfs_static_entries_lookup(kernel_entries, arraylen(kernel_entries), dentry->name);
		break;
	case INODE_SLAB_DIR:
		foreach(node, slab_get_list()) {
			slab_cache_t *slab = container_of(node, slab_cache_t, node);
			if (!strcmp(slab->name, dentry->name)) {
				child_inode = sysfs_new_inode(INODE_SLAB, slab, VFS_FILE);
				break;
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
		if (index == 0) {
			strcpy(dirent->d_name, ".");
		} else {
			strcpy(dirent->d_name, "..");
		}
		dirent->d_ino = inode->type;
		dirent->d_type = DT_DIR;
		return 0;
	}
	index -= 2;

	switch (inode->type) {
	case INODE_ROOT:
		return sysfs_static_entries_readdir(root_entries, arraylen(root_entries), index, dirent);
	case INODE_BUS_DIR:
		hashmap_foreach(key, element, &devices) {
			(void)key;
			bus_t *bus = container_of(element, bus_t, device);
			if (bus->device.type != DEVICE_BUS) continue;
			if (index == 0) {
				strcpy(dirent->d_name, bus->device.name);
				dirent->d_type = DT_DIR;
				return 0;
			}
			index--;
		}
		return -ENOENT;
	case INODE_BUS:
		bus_t *bus = inode->ptr;
		foreach(node, &bus->addresses) {
			if (index != 0) {
				index--;
				continue;
			}
			bus_addr_t *addr = container_of(node, bus_addr_t, node);
			strcpy(dirent->d_name, addr->name);
			dirent->d_type = DT_DIR;
			return 0;
		}
		return -ENOENT;
	case INODE_KERNEL_DIR:
		return sysfs_static_entries_readdir(kernel_entries, arraylen(kernel_entries), index, dirent);
	case INODE_SLAB_DIR:
		foreach(node, slab_get_list()) {
			if (index != 0) {
				index--;
				continue;
			}
			slab_cache_t *slab = container_of(node, slab_cache_t, node);
			strcpy(dirent->d_name, slab->name);
			dirent->d_type = DT_REG;
			return 0;
		}
		return -ENOENT;
	default:
		return -ENOSYS;
	}
}

static int sysfs_open(vfs_fd_t *fd) {
	fd->ops = &sysfs_fd_ops;
	return 0;
}

static int sysfs_getattr(vfs_node_t *vnode, struct stat *stat) {
	// allow execute perm only on directories
	if (vnode->flags == VFS_DIR) {
		stat->st_mode = 0555;
	} else {
		stat->st_mode = 0444;
	}

	return 0;
}

static void sysfs_cleanup(vfs_node_t *vnode) {
	sysfs_inode_t *inode = container_of(vnode, sysfs_inode_t, node);
	slab_free(inode);
}

static ssize_t sysfs_read(vfs_fd_t *fd, void *buf, off_t offset, size_t count) {
	sysfs_inode_t *inode = container_of(fd->inode, sysfs_inode_t, node);
	char str[4096];
	switch (inode->type) {
	case INODE_SLAB:;
		slab_cache_t *slab = inode->ptr;
		sprintf(str, "object size : %ld\n"
			"free count : %zu\n"
			"partial count : %zu\n"
			"full count : %zu\n", 
			slab->size, slab->free.node_count,
			slab->partial.node_count, slab->full.node_count);
		break;
	case INODE_MEM:
		sprintf(str, "total pages count : %zu\n"
			"used pages count : %zu\n"
			"prviate pages count : %zu\n"
			"shared pages count : %zu\n"
			"counter count : 4\n",
			pmm_get_total_pages(),
			pmm_get_used_pages(),
			pmm_get_private_pages(),
			pmm_get_shared_pages());
		break;
	default:
		return -ENOSYS;
	}

	if ((size_t)offset >= strlen(str)) return 0;
	if (offset + count > strlen(str)) count = strlen(str) - offset;
	memcpy(buf, str, count);
	return count;
}

static vfs_inode_ops_t sysfs_inode_ops = {
	.lookup  = sysfs_lookup,
	.readdir = sysfs_readdir,
	.getattr = sysfs_getattr,
	.cleanup = sysfs_cleanup,
	.open    = sysfs_open,
};

static vfs_fd_ops_t sysfs_fd_ops = {
	.read = sysfs_read,
};

static int sysfs_mount(const char *source, const char *target, unsigned long flags, const void *data, vfs_superblock_t **out_superblock) {
	(void)source;
	(void)target;
	(void)flags;
	(void)data;

	vfs_superblock_t *superblock = kmalloc(sizeof(vfs_superblock_t));
	memset(superblock, 0, sizeof(vfs_superblock_t));
	sysfs_inode_t *root = sysfs_new_inode(INODE_ROOT, NULL, VFS_DIR);
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
	slab_init(&sysfs_inodes_cache, sizeof(sysfs_inode_t), "sysfs-nodes");
	vfs_register_fs(&sysfs_filesystem);
	kok();
}