#include <kernel/tmpfs.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/cache.h>
#include <kernel/print.h>
#include <kernel/time.h>
#include <kernel/list.h>
#include <errno.h>

static vfs_node_t *inode2node(tmpfs_inode_t *inode);

#define INODE_NUMBER(inode) ((ino_t)((uintptr_t)inode) - MEM_KHEAP_START)

#define IS_DEV(flags) (flags & (TMPFS_TYPE_CHAR | TMPFS_TYPE_BLOCK))

// page cache ops
static int tmpfs_cache_read(cache_t *cache, off_t offset, size_t size, cache_callback_t callback, void *arg) {
	uintptr_t end = offset + size;
	for (uintptr_t addr=offset; addr<end; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		memset((void*)(kernel->hhdm + page), 0, PAGE_SIZE);
	}
	cache_call_callback(cache, callback, arg);
	return 0;
}

static cache_ops_t tmpfs_cache_ops = {
	.read = tmpfs_cache_read,
};

static tmpfs_inode_t *new_inode(long type) {
	tmpfs_inode_t *inode = kmalloc(sizeof(tmpfs_inode_t));
	memset(inode, 0, sizeof(tmpfs_inode_t));
	switch (type) {
	case TMPFS_TYPE_FILE:
		init_cache(&inode->cache);
		inode->cache.ops = &tmpfs_cache_ops;
		break;
	case TMPFS_TYPE_DIR:
		init_list(&inode->entries);
		break;
	}
	inode->type = type;
	inode->parent = NULL;
	inode->link_count = 1;

	//set times
	inode->atime = NOW();
	inode->ctime = NOW();
	inode->mtime = NOW();
	return inode;
}

static void free_inode(tmpfs_inode_t *inode) {
	switch (inode->type) {
	case TMPFS_TYPE_FILE:
		free_cache(&inode->cache);
		break;
	case TMPFS_TYPE_DIR:
		destroy_list(&inode->entries);
		break;
	case TMPFS_TYPE_LINK:
		kfree(inode->buffer);
		break;
	}
	kfree(inode);
}


static int tmpfs_mount(const char *source, const char *target, unsigned long flags, const void *data) {
	(void)data;
	(void)source;
	(void)flags;

	return vfs_mount(target, new_tmpfs());
}

static vfs_filesystem_t tmpfs = {
	.name = "tmpfs",
	.mount = tmpfs_mount,
};

void init_tmpfs() {
	kstatusf("init tmpfs... ");
	vfs_register_fs(&tmpfs);
	kok();
}

vfs_node_t *new_tmpfs() {
	tmpfs_inode_t *root_inode = new_inode(TMPFS_TYPE_DIR);
	root_inode->link_count = 0; //so it get freed when the tmpfs is unmounted
	return inode2node(root_inode);
}


static vfs_node_t *tmpfs_lookup(vfs_node_t *node, const char *name) {
	tmpfs_inode_t *inode = (tmpfs_inode_t *)node->private_inode;
	if (!strcmp(name, ".")) {
		return inode2node(inode);
	}

	if (!strcmp(name, "..")) {
		if (inode->parent)
			return inode2node(inode->parent);
		return inode2node(inode);
	}

	foreach(node, &inode->entries) {
		tmpfs_dirent_t *entry = (tmpfs_dirent_t *)node;
		if (!strcmp(name, entry->name)) {
			return inode2node(entry->inode);
		}
	}

	return NULL;
}

static ssize_t tmpfs_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	tmpfs_inode_t *inode = (tmpfs_inode_t *)fd->private;

	//update atime
	inode->atime = NOW();

	return cache_read(&inode->cache, buffer, offset, count);
}

static int tmpfs_truncate(vfs_node_t *node, size_t size) {
	tmpfs_inode_t *inode = (tmpfs_inode_t *)node->private_inode;
	
	// update mtime
	inode->mtime = NOW();
	
	return cache_truncate(&inode->cache, size);
}

static ssize_t tmpfs_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	tmpfs_inode_t *inode = (tmpfs_inode_t *)fd->private;

	// if the write is out of bound make the file bigger
	if (offset + count > inode->cache.size) {
		tmpfs_truncate(fd->inode, offset + count);
	}

	// update mtime
	inode->mtime = NOW();
	
	return cache_write(&inode->cache, buffer, offset, count);
}

static int tmpfs_unlink(vfs_node_t *node, const char *name) {
	kdebugf("unlink %s\n", name);
	tmpfs_inode_t *inode = (tmpfs_inode_t *)node->private_inode;

	tmpfs_dirent_t *entry = NULL;
	foreach(node, &inode->entries) {
		tmpfs_dirent_t *cur_entry = (tmpfs_dirent_t *)node;
		if (!strcmp(cur_entry->name, name)) {
			entry = cur_entry;
			break;
		}
	}

	if (!entry)return ENOENT;

	list_remove(&inode->entries, &entry->node);
	if ((--entry->inode->link_count) == 0 && entry->inode->open_count == 0) {
		//nobody uses it we can free
		free_inode(entry->inode);
	}
	kfree(entry);

	return 0;
}

static int tmpfs_exist(tmpfs_inode_t *inode, const char *name) {
	foreach(node, &inode->entries) {
		tmpfs_dirent_t *entry = (tmpfs_dirent_t *)node;
		if (!strcmp(name, entry->name)) {
			return 1;
		}
	}
	return 0;
}

static int tmpfs_link(vfs_node_t *parent_src, const char *src, vfs_node_t *parent_dest, const char *dest) {
	kdebugf("link %s %s\n", src, dest);
	tmpfs_inode_t *parent_src_inode  = (tmpfs_inode_t *)parent_src->private_inode;
	tmpfs_inode_t *parent_dest_inode = (tmpfs_inode_t *)parent_dest->private_inode;

	if (tmpfs_exist(parent_dest_inode, dest))return -EEXIST;

	tmpfs_inode_t *src_inode = NULL;
	foreach(node, &parent_src_inode->entries) {
		tmpfs_dirent_t *entry = (tmpfs_dirent_t *)node;
		if (!strcmp(entry->name, src)) {
			src_inode = entry->inode;
			break;
		}
	}
	if (!src_inode)return -ENOENT;

	//create new entry
	src_inode->link_count++;
	tmpfs_dirent_t *entry = kmalloc(sizeof(tmpfs_dirent_t));
	strcpy(entry->name, dest);
	entry->inode = src_inode;
	list_append(&parent_dest_inode->entries, &entry->node);
	return 0;
}

static int tmpfs_symlink(vfs_node_t *node, const char *name, const char *target) {
	tmpfs_inode_t *inode = (tmpfs_inode_t *)node->private_inode;

	if (tmpfs_exist(inode, name))return -EEXIST;

	tmpfs_inode_t *symlink = new_inode(TMPFS_TYPE_LINK);
	symlink->buffer_size = strlen(target);
	symlink->buffer = strdup(target);

	//create new entry
	tmpfs_dirent_t *entry = kmalloc(sizeof(tmpfs_dirent_t));
	strcpy(entry->name, name);
	entry->inode = symlink;
	list_append(&inode->entries, &entry->node);

	return 0;
}

static ssize_t tmpfs_readlink(vfs_node_t *node, char *buf, size_t bufsize) {
	tmpfs_inode_t *inode = (tmpfs_inode_t *)node->private_inode;
	if (bufsize > inode->buffer_size) bufsize = inode->buffer_size;

	memcpy(buf, inode->buffer, bufsize);
	return bufsize;
}

static int tmpfs_readdir(vfs_fd_t *fd, unsigned long index, struct dirent *dirent) {
	tmpfs_inode_t *inode = (tmpfs_inode_t *)fd->private;

	//update atime
	inode->atime = NOW();

	if (index == 0) {
		strcpy(dirent->d_name, ".");
		dirent->d_ino = INODE_NUMBER(inode);
		dirent->d_type = DT_DIR;
		return 0;
	}

	if (index == 1) {
		strcpy(dirent->d_name, "..");
		dirent->d_ino = INODE_NUMBER(inode->parent);
		dirent->d_type = DT_DIR;
		return 0;
	}

	index -=2;
	foreach(node, &inode->entries) {
		if (!index) {
			tmpfs_dirent_t *entry = (tmpfs_dirent_t *)node;
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

static void tmpfs_cleanup(vfs_node_t *node) {
	tmpfs_inode_t *inode = (tmpfs_inode_t *)node->private_inode;
	inode->open_count--;
	if (inode->open_count == 0 && inode->link_count == 0) {
		//nobody use it
		free_inode(inode);
	}
}

int tmpfs_create(vfs_node_t *node, const char *name, mode_t perm, long type, void *arg) {
	tmpfs_inode_t *inode = (tmpfs_inode_t *)node->private_inode;
	if (tmpfs_exist(inode, name))return -EEXIST;

	//turn vfs flag into tmpfs flags
	long inode_type = 0;
	switch (type) {
	case VFS_FILE:
		inode_type = TMPFS_TYPE_FILE;
		break;
	case VFS_SOCK:
		inode_type = TMPFS_TYPE_SOCK;
		break;
	case VFS_DIR:
		inode_type = TMPFS_TYPE_DIR;
		break;
	case VFS_CHAR:
		inode_type = TMPFS_TYPE_CHAR;
		break;
	case VFS_BLOCK:
		inode_type = TMPFS_TYPE_BLOCK;
		break;
	}

	//create new inode
	tmpfs_inode_t *child_inode = new_inode(inode_type);
	child_inode->link_count = 1;
	child_inode->parent = inode;
	child_inode->perm = perm;
	child_inode->data = arg;
	if ((type == VFS_BLOCK) || (type == VFS_CHAR)) {
		child_inode->dev = *(dev_t *)arg;
	}

	//create new entry
	tmpfs_dirent_t *entry = kmalloc(sizeof(tmpfs_dirent_t));
	memset(entry, 0, sizeof(tmpfs_dirent_t));
	strcpy(entry->name, name);
	entry->inode = child_inode;
	list_append(&inode->entries, &entry->node);

	return 0;
}

static int tmpfs_setattr(vfs_node_t *node, struct stat *st) {
	tmpfs_inode_t *inode = (tmpfs_inode_t *)node->private_inode;
	inode->perm         = st->st_mode & 0xffff;
	inode->owner        = st->st_uid;
	inode->group_owner  = st->st_gid;
	inode->atime        = st->st_atime;
	inode->mtime        = st->st_mtime;
	inode->ctime        = st->st_ctime;
	return 0;
}

static int tmpfs_getattr(vfs_node_t *node, struct stat *st) {
	tmpfs_inode_t *inode = (tmpfs_inode_t *)node->private_inode;
	st->st_size        = inode->type == TMPFS_TYPE_FILE ? inode->cache.size : 0;
	st->st_mode        = inode->perm;
	st->st_uid         = inode->owner;
	st->st_gid         = inode->group_owner;
	st->st_atime       = inode->atime;
	st->st_mtime       = inode->mtime;
	st->st_ctime       = inode->ctime;
	st->st_nlink       = inode->link_count;
	st->st_rdev        = inode->dev;
	st->st_ino         = INODE_NUMBER(inode); // fake an inode number

	//simulate fake blocks of 512 bytes
	//because blocks don't exist on tmpfs
	st->st_blksize = 512;
	st->st_blocks = (st->st_size + st->st_blksize - 1) / st->st_blksize;
	return 0;
}

static vfs_ops_t tmfps_ops = {
	.lookup     = tmpfs_lookup,
	.readdir    = tmpfs_readdir,
	.create     = tmpfs_create,
	.unlink     = tmpfs_unlink,
	.link       = tmpfs_link,
	.symlink    = tmpfs_symlink,
	.readlink   = tmpfs_readlink,
	.read       = tmpfs_read,
	.write      = tmpfs_write,
	.truncate   = tmpfs_truncate,
	.getattr    = tmpfs_getattr,
	.setattr    = tmpfs_setattr,
	.cleanup    = tmpfs_cleanup,
};

static vfs_node_t *inode2node(tmpfs_inode_t *inode) {
	vfs_node_t *node = kmalloc(sizeof(vfs_node_t));
	memset(node, 0, sizeof(vfs_node_t));
	node->private_inode = (void *)inode;
	node->flags = 0;
	node->ops = &tmfps_ops;

	switch (inode->type) {
	case TMPFS_TYPE_DIR:
		node->flags |= VFS_DIR;
		break;
	case TMPFS_TYPE_FILE:
		node->flags |= VFS_FILE;
		break;
	case TMPFS_TYPE_SOCK:
		node->private_inode2 = inode->data;
		node->flags |= VFS_SOCK;
		break;
	case TMPFS_TYPE_CHAR:
		node->flags |= VFS_CHAR;
		break;
	case TMPFS_TYPE_BLOCK:
		node->flags |= VFS_BLOCK;
		break;
	}

	inode->open_count++;
	return node;
}
