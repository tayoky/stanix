#include <kernel/cache.h>
#include <kernel/kernel.h>
#include <kernel/kheap.h>
#include <kernel/list.h>
#include <kernel/print.h>
#include <kernel/slab.h>
#include <kernel/string.h>
#include <kernel/time.h>
#include <kernel/tmpfs.h>
#include <errno.h>

static slab_cache_t tmpfs_inode_slab;
static slab_cache_t tmpfs_entry_slab;
static vfs_inode_ops_t tmpfs_inode_ops;

#define INODE_NUMBER(inode) ((ino_t)((uintptr_t)inode) - MEM_KHEAP_START)

// page cache ops
static int tmpfs_cache_read(cache_t *cache, off_t offset, size_t size) {
	uintptr_t end = offset + size;
	for (uintptr_t addr = offset; addr < end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		memset(mmu_phys2virt(page), 0, PAGE_SIZE);
	}
	cache_read_terminate(cache, offset, size);
	return 0;
}

static cache_ops_t tmpfs_cache_ops = {
	.read = tmpfs_cache_read,
};

static tmpfs_inode_t *new_inode(vfs_superblock_t *superblock, long type) {
	tmpfs_inode_t *inode = slab_alloc(&tmpfs_inode_slab);
	memset(inode, 0, sizeof(tmpfs_inode_t));
	vfs_init_created_node(&inode->vnode);
	inode->vnode.superblock = superblock;
	inode->vnode.ops        = &tmpfs_inode_ops;
	switch (type) {
	case TMPFS_TYPE_FILE:
		inode->vnode.flags |= VFS_FILE;
		init_cache(&inode->cache);
		inode->cache.ops = &tmpfs_cache_ops;
		break;
	case TMPFS_TYPE_DIR:
		inode->vnode.flags |= VFS_DIR;
		list_init(&inode->entries);
		break;
	case TMPFS_TYPE_SOCK:
		inode->vnode.flags |= VFS_SOCK;
		break;
	case TMPFS_TYPE_CHAR:
		inode->vnode.flags |= VFS_CHAR;
		break;
	case TMPFS_TYPE_BLOCK:
		inode->vnode.flags |= VFS_BLOCK;
		break;
	case TMPFS_TYPE_LINK:
		inode->vnode.flags |= VFS_LINK;
		break;
	}
	inode->type       = type;
	inode->parent     = NULL;
	inode->link_count = 0;
	inode->vnode.mode = 0555;
	inode->vnode.number = INODE_NUMBER(inode);

	return inode;
}

static int tmpfs_add_entry(tmpfs_inode_t *dir, tmpfs_inode_t *child, vfs_dentry_t *dentry) {
	// create new entry
	child->link_count++;
	tmpfs_dirent_t *entry = slab_alloc(&tmpfs_entry_slab);
	strcpy(entry->name, dentry->name);
	entry->inode = child;
	list_append(&dir->entries, &entry->node);
	return 0;
}

static tmpfs_dirent_t *tmpfs_get_entry(tmpfs_inode_t *dir, vfs_dentry_t *dentry) {
	foreach (node, &dir->entries) {
		tmpfs_dirent_t *current_entry = container_of(node, tmpfs_dirent_t, node);
		if (!strcmp(current_entry->name, dentry->name)) {
			return current_entry;
		}
	}
	return NULL;
}

static void tmpfs_remove_entry(tmpfs_inode_t *dir, tmpfs_dirent_t *entry) {
	list_remove(&dir->entries, &entry->node);
	if ((--entry->inode->link_count) == 0) {
		// nobody uses it we can release the inode
		vfs_node_release(&entry->inode->vnode);
	}
	slab_free(entry);
}

static int tmpfs_mount(const char *source, const char *target, unsigned long flags, const void *data, vfs_superblock_t **superblock_out) {
	(void)data;
	(void)source;
	(void)flags;
	(void)target;

	*superblock_out = new_tmpfs();
	return 0;
}

static vfs_filesystem_t tmpfs = {
	.name  = "tmpfs",
	.mount = tmpfs_mount,
};

void init_tmpfs() {
	kstatusf("init tmpfs... ");
	slab_init(&tmpfs_inode_slab, sizeof(tmpfs_inode_t), "tmpfs-inode");
	slab_init(&tmpfs_entry_slab, sizeof(tmpfs_dirent_t), "tmpfs-entry");
	vfs_register_fs(&tmpfs);
	kok();
}

vfs_superblock_t *new_tmpfs(void) {
	vfs_superblock_t *superblock = kmalloc(sizeof(vfs_superblock_t));
	memset(superblock, 0, sizeof(vfs_superblock_t));

	tmpfs_inode_t *root_inode   = new_inode(superblock, TMPFS_TYPE_DIR);
	root_inode->link_count      = 0; // so it get freed when the tmpfs is unmounted
	superblock->root            = &root_inode->vnode;
	superblock->root->ref_count = 1;
	return superblock;
}


static int tmpfs_lookup(vfs_node_t *vnode, vfs_dentry_t *dentry) {
	tmpfs_inode_t *inode = container_of(vnode, tmpfs_inode_t, vnode);
	if (!strcmp(dentry->name, ".")) {
		dentry->inode        = vfs_node_ref(&inode->vnode);
		dentry->inode_number = INODE_NUMBER(inode);
		return 0;
	}

	if (!strcmp(dentry->name, "..")) {
		if (inode->parent) {
			dentry->inode            = vfs_node_ref(&inode->parent->vnode);
			dentry->inode->ref_count = 1;
			dentry->inode_number     = INODE_NUMBER(inode->parent);
		} else {
			dentry->inode            = vfs_node_ref(&inode->vnode);
			dentry->inode->ref_count = 1;
			dentry->inode_number     = INODE_NUMBER(inode);
		}
		return 0;
	}

	tmpfs_dirent_t *entry = tmpfs_get_entry(inode, dentry);
	if (!entry) return -ENOENT;
	dentry->inode            = vfs_node_ref(&entry->inode->vnode);
	dentry->inode->ref_count = 1;
	dentry->inode_number     = INODE_NUMBER(entry->inode);
	return 0;
}

static ssize_t tmpfs_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	tmpfs_inode_t *inode = (tmpfs_inode_t *)fd->private;
	return cache_read(&inode->cache, buffer, offset, count);
}

static int tmpfs_truncate(vfs_node_t *vnode, size_t size) {
	tmpfs_inode_t *inode = container_of(vnode, tmpfs_inode_t, vnode);
	return cache_truncate(&inode->cache, size);
}

static ssize_t tmpfs_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	tmpfs_inode_t *inode = (tmpfs_inode_t *)fd->private;

	// if the write is out of bound make the file bigger
	if (offset + count > inode->cache.size) {
		tmpfs_truncate(fd->inode, offset + count);
	}
	return cache_write(&inode->cache, buffer, offset, count);
}

static int tmpfs_mmap(vfs_fd_t *fd, off_t offset, vmm_seg_t *seg) {
	tmpfs_inode_t *inode = (tmpfs_inode_t *)fd->private;

	return cache_mmap(&inode->cache, offset, seg);
}

static int tmpfs_exist(tmpfs_inode_t *inode, vfs_dentry_t *dentry) {
	return tmpfs_get_entry(inode, dentry) != NULL;
}

static ssize_t tmpfs_readlink(vfs_node_t *vnode, char *buf, size_t bufsize) {
	tmpfs_inode_t *inode = container_of(vnode, tmpfs_inode_t, vnode);
	if (bufsize > inode->buffer_size) bufsize = inode->buffer_size;

	memcpy(buf, inode->buffer, bufsize);
	return bufsize;
}

static int tmpfs_readdir(vfs_node_t *vnode, unsigned long index, struct dirent *dirent) {
	tmpfs_inode_t *inode = container_of(vnode, tmpfs_inode_t, vnode);

	if (index == 0) {
		strcpy(dirent->d_name, ".");
		dirent->d_ino  = INODE_NUMBER(inode);
		dirent->d_type = DT_DIR;
		return 0;
	}

	if (index == 1) {
		strcpy(dirent->d_name, "..");
		dirent->d_ino  = INODE_NUMBER(inode->parent);
		dirent->d_type = DT_DIR;
		return 0;
	}

	index -= 2;
	foreach (node, &inode->entries) {
		if (!index) {
			tmpfs_dirent_t *entry = container_of(node, tmpfs_dirent_t, node);
			strcpy(dirent->d_name, entry->name);
			dirent->d_ino = INODE_NUMBER(entry->inode);
			switch (entry->inode->type) {
			case TMPFS_TYPE_DIR:
				dirent->d_type = DT_DIR;
				break;
			case TMPFS_TYPE_FILE:
				dirent->d_type = DT_REG;
				break;
			case TMPFS_TYPE_LINK:
				dirent->d_type = DT_LNK;
				break;
			case TMPFS_TYPE_SOCK:
				dirent->d_type = DT_SOCK;
				break;
			case TMPFS_TYPE_CHAR:
				dirent->d_type = DT_CHR;
				break;
			case TMPFS_TYPE_BLOCK:
				dirent->d_type = DT_BLK;
				break;
			default:
				dirent->d_type = DT_UNKNOWN;
			}
			return 0;
		}
		index--;
	}
	return -ENOENT;
}

static void tmpfs_cleanup(vfs_node_t *vnode) {
	tmpfs_inode_t *inode = container_of(vnode, tmpfs_inode_t, vnode);
	switch (inode->type) {
	case TMPFS_TYPE_FILE:
		free_cache(&inode->cache);
		break;
	case TMPFS_TYPE_DIR:
		list_destroy(&inode->entries);
		break;
	case TMPFS_TYPE_LINK:
		kfree(inode->buffer);
		break;
	}
	slab_free(inode);
}

static int tmpfs_create(vfs_node_t *vnode, vfs_dentry_t *dentry, mode_t mode) {
	tmpfs_inode_t *inode = container_of(vnode, tmpfs_inode_t, vnode);
	if (tmpfs_exist(inode, dentry)) return -EEXIST;

	// create new inode
	tmpfs_inode_t *child_inode = new_inode(vnode->superblock, TMPFS_TYPE_FILE);
	child_inode->parent        = inode;
	child_inode->vnode.mode    = mode;

	tmpfs_add_entry(inode, child_inode, dentry);

	return 0;
}

static int tmpfs_mkdir(vfs_node_t *vnode, vfs_dentry_t *dentry, mode_t mode) {
	tmpfs_inode_t *inode = container_of(vnode, tmpfs_inode_t, vnode);
	if (tmpfs_exist(inode, dentry)) return -EEXIST;

	// create new inode
	tmpfs_inode_t *child_inode = new_inode(vnode->superblock, TMPFS_TYPE_DIR);
	child_inode->parent        = inode;
	child_inode->vnode.mode    = mode;

	tmpfs_add_entry(inode, child_inode, dentry);

	return 0;
}

static int tmpfs_mknod(vfs_node_t *vnode, vfs_dentry_t *dentry, mode_t mode, dev_t dev) {
	tmpfs_inode_t *inode = container_of(vnode, tmpfs_inode_t, vnode);
	if (tmpfs_exist(inode, dentry)) return -EEXIST;

	int type = 0;
	if (S_ISBLK(mode)) {
		type = TMPFS_TYPE_BLOCK;
	} else if (S_ISCHR(mode)) {
		type = TMPFS_TYPE_CHAR;
	} else if (S_ISSOCK(mode)) {
		type = TMPFS_TYPE_SOCK;
	} else {
		return -EINVAL;
	}

	// create new inode
	tmpfs_inode_t *child_inode = new_inode(vnode->superblock, type);
	child_inode->parent        = inode;
	child_inode->vnode.mode    = mode;
	child_inode->dev           = dev;

	tmpfs_add_entry(inode, child_inode, dentry);

	return 0;
}

static int tmpfs_link(vfs_dentry_t *old_dentry, vfs_node_t *new_dir, vfs_dentry_t *new_dentry) {
	kdebugf("link %s %s\n", old_dentry->name, new_dentry->name);
	tmpfs_inode_t *new_dir_inode = container_of(new_dir, tmpfs_inode_t, vnode);
	tmpfs_inode_t *old_inode     = container_of(old_dentry->inode, tmpfs_inode_t, vnode);

	if (tmpfs_exist(new_dir_inode, new_dentry)) return -EEXIST;

	tmpfs_add_entry(new_dir_inode, old_inode, new_dentry);
	return 0;
}

static int tmpfs_symlink(vfs_node_t *vnode, vfs_dentry_t *dentry, const char *target) {
	tmpfs_inode_t *inode = container_of(vnode, tmpfs_inode_t, vnode);
	if (tmpfs_exist(inode, dentry)) return -EEXIST;

	tmpfs_inode_t *symlink = new_inode(vnode->superblock, TMPFS_TYPE_LINK);
	symlink->buffer_size   = strlen(target);
	symlink->buffer        = strdup(target);

	tmpfs_add_entry(inode, symlink, dentry);

	return 0;
}

static int tmpfs_rename(vfs_node_t *old_vnode, vfs_dentry_t *old_dentry, vfs_node_t *new_vnode, vfs_dentry_t *new_dentry, unsigned int flags) {
	(void)flags;
	kdebugf("rename %s to %s\n", old_dentry->name, new_dentry->name);
	tmpfs_inode_t *old_parent = container_of(old_vnode, tmpfs_inode_t, vnode);
	tmpfs_inode_t *new_parent = container_of(new_vnode, tmpfs_inode_t, vnode);
	tmpfs_dirent_t *old_entry = tmpfs_get_entry(old_parent, old_dentry);

	tmpfs_add_entry(new_parent, old_entry->inode, new_dentry);
	tmpfs_remove_entry(old_parent, old_entry);

	return 0;
}

static int tmpfs_unlink(vfs_node_t *vnode, vfs_dentry_t *dentry) {
	kdebugf("unlink %s\n", dentry->name);
	tmpfs_inode_t *inode = container_of(vnode, tmpfs_inode_t, vnode);

	tmpfs_dirent_t *entry = tmpfs_get_entry(inode, dentry);
	if (!entry) return ENOENT;
	if (entry->inode->type == TMPFS_TYPE_DIR) return -EISDIR;

	tmpfs_remove_entry(inode, entry);

	return 0;
}

static int tmpfs_rmdir(vfs_node_t *vnode, vfs_dentry_t *dentry) {
	kdebugf("rmdir %s\n", dentry->name);
	tmpfs_inode_t *inode = container_of(vnode, tmpfs_inode_t, vnode);

	tmpfs_dirent_t *entry = tmpfs_get_entry(inode, dentry);
	if (!entry) return ENOENT;
	if (entry->inode->type != TMPFS_TYPE_DIR) return -ENOTDIR;

	// check if the directory is empty
	tmpfs_inode_t *child_inode = entry->inode;
	if (child_inode->entries.first_node) return -ENOTEMPTY;

	tmpfs_remove_entry(inode, entry);

	return 0;
}

static int tmpfs_setattr(vfs_node_t *vnode, struct stat *st, int mask) {
	tmpfs_inode_t *inode = container_of(vnode, tmpfs_inode_t, vnode);
	(void)inode;
	(void)st;
	(void)mask;
	// we rely on the vfs's cache to store attributes
	return 0;
}

static int tmpfs_getattr(vfs_node_t *vnode, struct stat *st) {
	tmpfs_inode_t *inode = container_of(vnode, tmpfs_inode_t, vnode);
	st->st_size          = inode->cache.size;
	st->st_nlink         = inode->link_count;
	st->st_rdev          = inode->dev;

	// simulate fake blocks of 512 bytes
	// because blocks don't exist on tmpfs
	st->st_blksize = 512;
	st->st_blocks  = (st->st_size + st->st_blksize - 1) / st->st_blksize;
	return 0;
}


static vfs_fd_ops_t tmpfs_fd_ops = {
	.read  = tmpfs_read,
	.write = tmpfs_write,
	.mmap  = tmpfs_mmap,
};

static int tmpfs_open(vfs_fd_t *fd) {
	tmpfs_inode_t *inode = container_of(fd->inode, tmpfs_inode_t, vnode);
	fd->ops              = &tmpfs_fd_ops;
	fd->private          = inode;
	return 0;
}

static vfs_inode_ops_t tmpfs_inode_ops = {
	.lookup   = tmpfs_lookup,
	.readdir  = tmpfs_readdir,
	.create   = tmpfs_create,
	.mkdir    = tmpfs_mkdir,
	.mknod    = tmpfs_mknod,
	.link     = tmpfs_link,
	.symlink  = tmpfs_symlink,
	.readlink = tmpfs_readlink,
	.rename   = tmpfs_rename,
	.unlink   = tmpfs_unlink,
	.rmdir    = tmpfs_rmdir,
	.truncate = tmpfs_truncate,
	.getattr  = tmpfs_getattr,
	.setattr  = tmpfs_setattr,
	.cleanup  = tmpfs_cleanup,
	.open     = tmpfs_open,
};
