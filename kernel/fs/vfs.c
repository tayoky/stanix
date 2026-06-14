#include <kernel/assert.h>
#include <kernel/device.h>
#include <kernel/kernel.h>
#include <kernel/kheap.h>
#include <kernel/list.h>
#include <kernel/panic.h>
#include <kernel/poll.h>
#include <kernel/print.h>
#include <kernel/process.h>
#include <kernel/refcount.h>
#include <kernel/slab.h>
#include <kernel/string.h>
#include <kernel/time.h>
#include <kernel/vfs.h>
#include <kernel/vmm.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stddef.h>

// TODO : make this process specific
vfs_dentry_t *root;

static list_t fs_types;
static list_t superblocks;
static list_t dentries_lru;
static slab_cache_t dentries_slab;
static slab_cache_t fd_slab;

static void vfs_add_dentry(vfs_dentry_t *parent, vfs_dentry_t *child) {
	// child hold a ref to the parent
	child->parent = vfs_dentry_ref(parent);
	list_append(&parent->children, &child->children_node);
}

static void vfs_remove_dentry(vfs_dentry_t *dentry) {
	if (!dentry->parent) return;
	list_remove(&dentry->parent->children, &dentry->children_node);
	// child hold a ref to the parent
	vfs_dentry_release(dentry->parent);
	dentry->parent = NULL;
}

static int dentry_constructor(slab_cache_t *cache, void *data) {
	(void)cache;
	vfs_dentry_t *dentry = data;
	memset(dentry, 0, sizeof(vfs_dentry_t));
	return 0;
}

static int dentry_destructor(slab_cache_t *cache, void *data) {
	(void)cache;
	vfs_dentry_t *dentry = data;
	kassert(dentry->ref_count == 0);
	vfs_node_release(dentry->inode);
	vfs_remove_dentry(dentry);
	return 0;
}

static void *dentry_evict(slab_cache_t *cache) {
	(void)cache;
	if (!dentries_lru.first_node) return NULL;
	vfs_dentry_t *dentry = container_of(dentries_lru.first_node, vfs_dentry_t, lru_node);
	list_remove(&dentries_lru, &dentry->lru_node);
	return dentry;
}

static void dentry_add_lru(vfs_dentry_t *dentry) {
	list_append(&dentries_lru, &dentry->lru_node);
}

static void dentry_remove_lru(vfs_dentry_t *dentry) {
	list_remove(&dentries_lru, &dentry->lru_node);
}

static int fd_constructor(slab_cache_t *cache, void *data) {
	(void)cache;
	vfs_fd_t *fd = data;
	memset(fd, 0, sizeof(vfs_fd_t));
	fd->ref_count = 1;
	return 0;
}

void init_vfs(void) {
	kstatusf("init vfs... ");
	slab_init(&fd_slab, sizeof(vfs_fd_t), "vfs-file-descriptors");
	fd_slab.constructor = fd_constructor;
	slab_init(&dentries_slab, sizeof(vfs_dentry_t), "vfs-dentries");
	dentries_slab.constructor = dentry_constructor;
	dentries_slab.destructor  = dentry_destructor;
	dentries_slab.evict       = dentry_evict;

	root            = slab_alloc(&dentries_slab);
	root->ref_count = 1;
	strcpy(root->name, "[root]");

	list_init(&fs_types);
	list_init(&superblocks);
	kok();
}

void vfs_register_fs(vfs_filesystem_t *fs) {
	list_append(&fs_types, &fs->node);
}

void vfs_unregister_fs(vfs_filesystem_t *fs) {
	list_remove(&fs_types, &fs->node);
}

void vfs_init_created_node(vfs_node_t *node) {
	node->uid   = get_current_euid();
	node->gid   = get_current_egid();
	node->atime = node->mtime = node->ctime = gettime_sec(CLOCK_REALTIME);
	node->ref_count                         = 1;
}

static int vfs_update_time(vfs_node_t *node, int mask) {
	struct stat st;
	st.st_atime = st.st_mtime = st.st_ctime = gettime_sec(CLOCK_REALTIME);
	return vfs_setattr(node, &st, mask);
}

// basename without modyfing anything
static const char *vfs_basename(const char *path) {
	const char *base = path + strlen(path) - 1;
	// path of directory might finish with '/' like /tmp/dir/
	if (*base == '/') base--;
	while (*base != '/') {
		base--;
		if (base < path) {
			break;
		}
	}
	base++;
	return base;
}

static int vfs_create_dentry(vfs_dentry_t *at, const char *path, vfs_dentry_t **_parent, vfs_dentry_t **_dentry) {
	vfs_dentry_t *parent = vfs_get_dentry_at(at, path, O_PARENT);
	if (IS_ERR(parent)) {
		return PTR2ERR(parent);
	}

	if (parent->inode->flags != VFS_DIR) {
		vfs_dentry_release(parent);
		return -ENOTDIR;
	}

	if (!(vfs_perm(parent->inode) & PERM_WRITE)) {
		return -EACCES;
	}

	vfs_dentry_t *dentry = slab_alloc(&dentries_slab);
	if (!dentry) {
		vfs_dentry_release(parent);
		return -ENOMEM;
	}
	strcpy(dentry->name, vfs_basename(path));
	dentry->ref_count = 0;

	*_parent = parent;
	*_dentry = dentry;

	return 0;
}

static void vfs_unlink_dentry(vfs_dentry_t *dentry) {
	vfs_remove_dentry(dentry);
	dentry->flags |= VFS_DENTRY_UNLINKED;
}

static void vfs_destroy_superblock(vfs_superblock_t *superblock) {
	if (!superblock) return;
	if (superblock->ops && superblock->ops->destroy) {
		superblock->ops->destroy(superblock);
	} else {
		vfs_close(superblock->device);
		vfs_node_release(superblock->root);
		kfree(superblock);
	}
}

int vfs_auto_mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data) {
	vfs_superblock_t *superblock = NULL;
	foreach (node, &fs_types) {
		vfs_filesystem_t *fs = (vfs_filesystem_t *)node;
		if (!strcmp(fs->name, filesystemtype)) {
			if (!fs->mount) {
				return -ENODEV;
			}
			int ret = fs->mount(source, target, mountflags, data, &superblock);
			if (ret < 0) return ret;
			if (!superblock) return ret;

			// mount the superblock
			ret                          = vfs_mount(target, superblock);
			superblock->root->superblock = superblock;
			if (ret < 0) {
				vfs_destroy_superblock(superblock);
			}
			return ret;
		}
	}

	return -ENODEV;
}

int vfs_mount_on(vfs_dentry_t *mount_point, vfs_superblock_t *superblock) {
	kdebugf("mount superblock on %s\n", mount_point->name);

	// create a new fake dentry for the root of the superblock
	vfs_dentry_t *root_dentry = slab_alloc(&dentries_slab);
	root_dentry->parent       = vfs_dentry_ref(mount_point->parent);
	memcpy(root_dentry->name, mount_point->name, sizeof(mount_point->name));
	root_dentry->inode     = vfs_node_ref(superblock->root);
	root_dentry->ref_count = 1;
	root_dentry->flags     = VFS_DENTRY_MOUNT;

	// make a new ref to the mount point to prevent it from being close
	root_dentry->old = vfs_dentry_ref(mount_point);

	// insert the new fake dentry at the place of the original one
	if (mount_point->parent) {
		list_remove(&mount_point->parent->children, &mount_point->children_node);
		list_append(&mount_point->parent->children, &root_dentry->children_node);
	} else if (mount_point == root) {
		// special case for root
		root = root_dentry;
	}
	return 0;
}

int vfs_mount_at(vfs_dentry_t *at, const char *name, vfs_superblock_t *superblock) {
	// first open the mount point
	vfs_dentry_t *mount_point = vfs_get_dentry_at(at, name, O_RDWR);
	if (IS_ERR(mount_point)) {
		return PTR2ERR(mount_point);
	}

	int ret = vfs_mount_on(mount_point, superblock);

	vfs_dentry_release(mount_point);

	return ret;
}

int vfs_unmount_at(vfs_dentry_t *at, const char *path) {
	vfs_dentry_t *mount_point = vfs_get_dentry_at(at, path, 0);
	if (IS_ERR(mount_point)) {
		return PTR2ERR(mount_point);
	}

	if (!(mount_point->flags & VFS_DENTRY_MOUNT)) {
		// not even a mount point
		vfs_dentry_release(mount_point);
		return -EINVAL;
	}

	vfs_destroy_superblock(mount_point->inode->superblock);

	// replace the fake dentry by the one before it
	vfs_dentry_t *parent = mount_point->parent;
	if (parent) {
		mount_point->parent = NULL;
		list_remove(&parent->children, &mount_point->children_node);
		list_append(&parent->children, &mount_point->old->children_node);
	}

	vfs_dentry_release(mount_point);
	return 0;
}

ssize_t vfs_read(vfs_fd_t *fd, void *buffer, uint64_t offset, size_t count) {
	if (fd->type == VFS_BLOCK || fd->type == VFS_CHAR) {
		// check if device is unplugged
		if (((device_t *)fd->private)->type == DEVICE_UNPLUGGED) return -ENODEV;
	} else if (fd->type & VFS_DIR) {
		return -EISDIR;
	}
	if (fd->flags & O_WRONLY) {
		return -EBADF;
	}

	if (fd->ops->read) {
		vfs_update_time(fd->inode, VNODE_ATTR_ATIME);
		return fd->ops->read(fd, buffer, offset, count);
	} else {
		return -EINVAL;
	}
}

ssize_t vfs_write(vfs_fd_t *fd, const void *buffer, uint64_t offset, size_t count) {
	if (fd->type == VFS_BLOCK || fd->type == VFS_CHAR) {
		// check if device is unplugged
		if (((device_t *)fd->private)->type == DEVICE_UNPLUGGED) return -ENODEV;
	} else if (fd->type & VFS_DIR) {
		return -EISDIR;
	}
	if (!(fd->flags & (O_WRONLY | O_RDWR))) {
		return -EBADF;
	}
	if (fd->ops->write) {
		vfs_update_time(fd->inode, VNODE_ATTR_MTIME);
		return fd->ops->write(fd, buffer, offset, count);
	} else {
		return -EINVAL;
	}
}

int vfs_ioctl(vfs_fd_t *fd, long request, void *arg) {
	if (!fd || !fd->ops->ioctl) return -EBADF;
	if (fd->type == VFS_BLOCK || fd->type == VFS_CHAR) {
		// check if device is unplugged
		if (((device_t *)fd->private)->type == DEVICE_UNPLUGGED) return -ENODEV;
	}
	return fd->ops->ioctl(fd, request, arg);
}

int vfs_poll_add(vfs_fd_t *fd, poll_event_t *event) {
	if (!fd) return -EBADF;
	if (!fd->ops->poll_add) {
		// by default files are always readable and writable
		event->revents = POLLIN | POLLOUT;
		return 0;
	}
	return fd->ops->poll_add(fd, event);
}

int vfs_poll_remove(vfs_fd_t *fd, poll_event_t *event) {
	if (!fd) return -EBADF;
	if (!fd->ops->poll_remove) return 0;
	return fd->ops->poll_remove(fd, event);
}

int vfs_poll_get(vfs_fd_t *fd, poll_event_t *event) {
	if (!fd) return -EBADF;
	if (!fd->ops->poll_get) {
		// by default files are always readable and writable
		event->revents = POLLIN | POLLOUT;
		return 0;
	}
	event->revents = 0;
	int ret        = fd->ops->poll_get(fd, event);
	// cap events
	event->revents &= event->events | POLLHUP | POLLNVAL | POLLHUP;
	return ret;
}

ssize_t vfs_readlink(vfs_node_t *node, char *buf, size_t bufsiz) {
	if (node->flags != VFS_LINK) {
		return -ENOLINK;
	}
	if (node->ops->readlink) {
		vfs_update_time(node, VNODE_ATTR_ATIME);
		return node->ops->readlink(node, buf, bufsiz);
	} else {
		return -EINVAL;
	}
}

vfs_dentry_t *vfs_lookup(vfs_dentry_t *entry, const char *name) {
	// check perm
	if (!(vfs_perm(entry->inode) & PERM_EXECUTE)) {
		return ERR2PTR(-EACCES);
	}

	// cannot do lookup on negative entry
	if (vfs_dentry_is_negative(entry)) {
		return ERR2PTR(-EINVAL);
	}

	if (entry->inode->flags != VFS_DIR) {
		return ERR2PTR(-ENOTDIR);
	}

	// handle .. here so we can handle the parent of mount point
	if ((!strcmp("..", name)) && entry->parent) {
		return vfs_dentry_ref(entry->parent);
	}

	if ((!strcmp(".", name))) {
		return vfs_dentry_ref(entry);
	}

	// first search in the dentries cache
	foreach (list_node, &entry->children) {
		vfs_dentry_t *current_entry = container_of(list_node, vfs_dentry_t, children_node);
		if (!strcmp(current_entry->name, name)) {
			// cached entries must not be negative
			kassert(!vfs_dentry_is_negative(current_entry));
			vfs_dentry_t *ret = vfs_dentry_ref(current_entry);

			// we might need to remove it from lru
			if (ret->ref_count == 1) {
				dentry_remove_lru(ret);
			}
			return ret;
		}
	}


	// it isen't chached
	// ask the fs for it
	if (!entry->inode->ops->lookup) {
		return ERR2PTR(-EINVAL);
	}

	vfs_dentry_t *child_entry = slab_alloc(&dentries_slab);
	strcpy(child_entry->name, name);
	child_entry->ref_count = 1;

	int ret = entry->inode->ops->lookup(entry->inode, child_entry);
	if (ret < 0) {
		slab_free(child_entry);
		return ERR2PTR(ret);
	}

	// link it in the dentry cache
	vfs_add_dentry(entry, child_entry);
	return child_entry;
}

void vfs_dentry_release(vfs_dentry_t *dentry) {
	while (dentry) {
		if (ref_count_dec(&dentry->ref_count) > 1) {
			return;
		}

		if (vfs_dentry_is_negative(dentry) || (dentry->inode->superblock->flags & VFS_SUPERBLOCK_NO_DCACHE)) {
			// we cannot cache
			vfs_dentry_t *parent = dentry->parent;
			if (parent == dentry) parent = NULL;

			// we cannot use vfs_remove_entry cause it call vfs_dentry_release
			if (parent) {
				list_remove(&parent->children, &dentry->children_node);
			}
			dentry->parent = NULL;
			slab_free(dentry);
			dentry = parent;
			continue;
		}

		// we can cache
		dentry_add_lru(dentry);
		return;
	}
}

void vfs_node_release(vfs_node_t *node) {
	if (!node) return;

	if (ref_count_dec(&node->ref_count) > 1) {
		return;
	}

	// we can finaly close

	if (node->ops->cleanup) {
		node->ops->cleanup(node);
	} else {
		kfree(node);
	}
}

int vfs_create_at(vfs_dentry_t *at, const char *path, mode_t mode) {
	vfs_dentry_t *parent;
	vfs_dentry_t *dentry;
	int ret = vfs_create_dentry(at, path, &parent, &dentry);
	if (ret < 0) return ret;

	if (!parent->inode->ops || !parent->inode->ops->create) {
		ret = -EINVAL;
		goto error;
	}
	ret = parent->inode->ops->create(parent->inode, dentry, mode);
	if (ret < 0) goto error;

	// now we can link the dentry if the fs filled it
	if (!vfs_dentry_is_negative(dentry)) {
		vfs_add_dentry(parent, dentry);
	}

error:
	vfs_dentry_release(parent);
	vfs_dentry_release(dentry);
	return ret;
}

int vfs_mkdir_at(vfs_dentry_t *at, const char *path, mode_t mode) {
	vfs_dentry_t *parent;
	vfs_dentry_t *dentry;
	int ret = vfs_create_dentry(at, path, &parent, &dentry);
	if (ret < 0) return ret;

	if (!parent->inode->ops || !parent->inode->ops->mkdir) {
		ret = -EINVAL;
		goto error;
	}
	ret = parent->inode->ops->mkdir(parent->inode, dentry, mode);
	if (ret < 0) goto error;

	// now we can link the dentry if the fs filled it
	if (!vfs_dentry_is_negative(dentry)) {
		vfs_add_dentry(parent, dentry);
	}

error:
	vfs_dentry_release(parent);
	vfs_dentry_release(dentry);
	return ret;
}

int vfs_mknod_at(vfs_dentry_t *at, const char *path, mode_t mode, dev_t dev) {
	vfs_dentry_t *parent;
	vfs_dentry_t *dentry;
	int ret = vfs_create_dentry(at, path, &parent, &dentry);
	if (ret < 0) return ret;

	if (!parent->inode->ops || !parent->inode->ops->mknod) {
		ret = -EINVAL;
		goto error;
	}
	ret = parent->inode->ops->mknod(parent->inode, dentry, mode, dev);
	if (ret < 0) goto error;

	// now we can link the dentry if the fs filled it
	if (!vfs_dentry_is_negative(dentry)) {
		vfs_add_dentry(parent, dentry);
	}

error:
	vfs_dentry_release(parent);
	vfs_dentry_release(dentry);
	return ret;
}

int vfs_link_at(vfs_dentry_t *old_at, const char *old_path, vfs_dentry_t *new_at, const char *new_path) {
	vfs_dentry_t *old_dentry = vfs_get_dentry_at(old_at, old_path, O_NOFOLLOW);
	if (IS_ERR(old_dentry)) return PTR2ERR(old_dentry);

	vfs_dentry_t *new_parent = NULL;
	vfs_dentry_t *new_dentry = NULL;
	int ret                  = vfs_create_dentry(new_at, new_path, &new_parent, &new_dentry);
	if (ret < 0) goto error;

	// hardlink cannot cross mount point boundaries
	if (old_dentry->inode->superblock != new_parent->inode->superblock) {
		ret = -EXDEV;
		goto error;
	}

	// call link on the parents
	if (!new_parent->inode->ops || !new_parent->inode->ops->link) {
		ret = -EINVAL;
		goto error;
	}
	ret = new_parent->inode->ops->link(old_dentry, new_parent->inode, new_dentry);
	if (ret < 0) goto error;

	// now we can link the dentry if the fs filled it
	if (!vfs_dentry_is_negative(new_dentry)) {
		vfs_add_dentry(new_parent, new_dentry);
	}

error:
	vfs_dentry_release(old_dentry);
	vfs_dentry_release(new_parent);
	vfs_dentry_release(new_dentry);
	return ret;
}

int vfs_symlink_at(const char *target, vfs_dentry_t *at, const char *path) {
	vfs_dentry_t *parent;
	vfs_dentry_t *dentry;
	int ret = vfs_create_dentry(at, path, &parent, &dentry);
	if (ret < 0) return ret;

	if (!parent->inode->ops || !parent->inode->ops->symlink) {
		ret = -EINVAL;
		goto error;
	}
	ret = parent->inode->ops->symlink(parent->inode, dentry, target);
	if (ret < 0) goto error;

error:
	vfs_dentry_release(parent);
	vfs_dentry_release(dentry);
	return ret;
}

int vfs_rename_at(vfs_dentry_t *old_at, const char *old_path, vfs_dentry_t *new_at, const char *new_path, unsigned int flags) {
	vfs_dentry_t *old_dentry = vfs_get_dentry_at(old_at, old_path, O_NOFOLLOW);
	if (IS_ERR(old_dentry)) return PTR2ERR(old_dentry);

	vfs_dentry_t *new_parent = NULL;
	vfs_dentry_t *new_dentry = NULL;
	int ret                  = vfs_create_dentry(new_at, new_path, &new_parent, &new_dentry);
	if (ret < 0) goto error;

	// rename cannot cross mount point boundaries
	if (old_dentry->inode->superblock != new_parent->inode->superblock) {
		ret = -EXDEV;
		goto error;
	}

	vfs_dentry_t *old_parent = old_dentry->parent;
	if (!old_parent) {
		// cannot rename root
		ret = -EINVAL;
		goto error;
	}

	// call rename on the parent
	if (!new_parent->inode->ops || !new_parent->inode->ops->rename) {
		ret = -EINVAL;
		goto error;
	}
	ret = new_parent->inode->ops->rename(old_parent->inode, old_dentry, new_parent->inode, new_dentry, flags);
	if (ret < 0) goto error;

	// now we can link the dentry if the fs filled it
	if (!vfs_dentry_is_negative(new_dentry)) {
		vfs_add_dentry(new_parent, new_dentry);
	}

error:
	vfs_dentry_release(old_dentry);
	vfs_dentry_release(new_parent);
	vfs_dentry_release(new_dentry);
	return ret;
}

int vfs_unlink_at(vfs_dentry_t *at, const char *path) {
	vfs_dentry_t *dentry = vfs_get_dentry_at(at, path, O_NOFOLLOW);
	if (IS_ERR(dentry)) {
		return PTR2ERR(dentry);
	}

	int ret = 0;

	// cannot unlink mount points
	if (dentry->flags & VFS_DENTRY_MOUNT) {
		ret = -EBUSY;
		goto error;
	}

	if (dentry->inode->flags == VFS_DIR) {
		ret = -EISDIR;
		goto error;
	}

	vfs_dentry_t *parent_entry = dentry->parent;
	if (!parent_entry) {
		// as far as i know you cannot unlink root
		ret = -ENOENT;
		goto error;
	}

	// permission checking
	struct stat parent_st;
	struct stat child_st;
	vfs_getattr(parent_entry->inode, &parent_st);
	vfs_getattr(dentry->inode, &child_st);
	if (parent_st.st_mode & S_ISVTX) {
		// special case for sticky bit
		if (parent_st.st_uid != get_current_euid() && child_st.st_uid != get_current_euid()) {
			return -EACCES;
		}

	} else {
		if (!(vfs_perm(parent_entry->inode) & PERM_WRITE)) {
			return -EACCES;
		}
	}

	// call unlink on the parent
	vfs_node_t *parent = parent_entry->inode;
	if (!parent->ops || !parent->ops->unlink) {
		ret = -EINVAL;
		goto error;
	}
	ret = parent->ops->unlink(parent, dentry);
	if (ret < 0) goto error;

	vfs_unlink_dentry(dentry);

error:
	vfs_dentry_release(dentry);
	return ret;
}


int vfs_rmdir_at(vfs_dentry_t *at, const char *path) {
	vfs_dentry_t *dentry = vfs_get_dentry_at(at, path, 0);
	if (IS_ERR(dentry)) {
		return PTR2ERR(dentry);
	}

	int ret = 0;

	// cannot rmdir mount points
	if (dentry->flags & VFS_DENTRY_MOUNT) {
		ret = -EBUSY;
		goto error;
	}

	if (dentry->inode->flags != VFS_DIR) {
		ret = -ENOTDIR;
		goto error;
	}

	vfs_dentry_t *parent_entry = dentry->parent;
	if (!parent_entry) {
		// as far as i know you cannot rmdir root
		ret = -ENOENT;
		goto error;
	}

	// permission checking
	struct stat parent_st;
	struct stat child_st;
	vfs_getattr(parent_entry->inode, &parent_st);
	vfs_getattr(dentry->inode, &child_st);
	if (parent_st.st_mode & S_ISVTX) {
		// special case for sticky bit
		if (parent_st.st_uid != get_current_euid() && child_st.st_uid != get_current_euid()) {
			return -EACCES;
		}

	} else {
		if (!(vfs_perm(parent_entry->inode) & PERM_WRITE)) {
			return -EACCES;
		}
	}

	// call rmdir on the parent
	vfs_node_t *parent = parent_entry->inode;
	if (!parent->ops || !parent->ops->rmdir) {
		ret = -EINVAL;
		goto error;
	}
	ret = parent->ops->rmdir(parent, dentry);
	if (ret < 0) goto error;

	vfs_unlink_dentry(dentry);

error:
	vfs_dentry_release(dentry);
	return ret;
}

int vfs_readdir(vfs_node_t *node, unsigned long index, struct dirent *dirent) {
	if (!node) {
		return -EINVAL;
	}
	if (!(node->flags & VFS_DIR)) {
		return -ENOTDIR;
	}
	dirent->d_type = DT_UNKNOWN;
	dirent->d_ino  = 1; // some programs want non NULL inode
	if (node->ops->readdir) {
		vfs_update_time(node, VNODE_ATTR_ATIME);
		return node->ops->readdir(node, index, dirent);
	} else {
		return -EINVAL;
	}
}

int vfs_getattr(vfs_node_t *node, struct stat *st) {
	if (!node) return -EINVAL;
	memset(st, 0, sizeof(struct stat));
	st->st_nlink = 1; // in case a driver forgot to set :D
	st->st_mode  = node->mode;
	st->st_atime = node->atime;
	st->st_mtime = node->mtime;
	st->st_ctime = node->ctime;
	st->st_uid   = node->uid;
	st->st_gid   = node->gid;
	st->st_ino   = node->number;
	// maybee we can sync
	if (node->ops && node->ops->getattr) {
		int ret = node->ops->getattr(node, st);
		if (ret < 0) return ret;
	}

	// file type to mode
	if (node->flags & VFS_FILE) {
		st->st_mode |= S_IFREG;
	} else if (node->flags & VFS_DIR) {
		st->st_mode |= S_IFDIR;
	} else if (node->flags & VFS_SOCK) {
		st->st_mode |= S_IFSOCK;
	} else if (node->flags & VFS_BLOCK) {
		st->st_mode |= S_IFBLK;
	} else if (node->flags & VFS_CHAR) {
		st->st_mode |= S_IFCHR;
	} else if (node->flags & VFS_LINK) {
		st->st_mode |= S_IFLNK;
	}
	return 0;
}

int vfs_setattr(vfs_node_t *node, struct stat *st, int mask) {
	// make sure we can actually sync
	if (!node || !node->ops || !node->ops->setattr) {
		return -EINVAL; // should be another error ... but what ???
	}
	if (mask & VNODE_ATTR_MODE) node->mode = st->st_mode;
	if (mask & VNODE_ATTR_UID) node->uid = st->st_uid;
	if (mask & VNODE_ATTR_GID) node->gid = st->st_gid;
	if (mask & VNODE_ATTR_ATIME) node->atime = st->st_atime;
	if (mask & VNODE_ATTR_MTIME) node->mtime = st->st_mtime;
	if (mask & VNODE_ATTR_CTIME) node->ctime = st->st_ctime;
	int ret = node->ops->setattr(node, st, mask);
	if (ret < 0) return ret;
	return ret;
}

int vfs_chroot(vfs_dentry_t *new_root) {
	root = new_root;
	return 0;
}

vfs_node_t *vfs_get_node_at(vfs_dentry_t *at, const char *path, long flags, ...) {
	mode_t mode = 0777;
	if (flags & O_CREAT) {
		va_list args;
		va_start(args, flags);
		mode = va_arg(args, mode_t);
		va_end(args);
	}
	vfs_dentry_t *dentry = vfs_get_dentry_at(at, path, flags, mode);
	if (IS_ERR(dentry)) return (vfs_node_t *)dentry;
	vfs_node_t *node = vfs_node_ref(dentry->inode);
	vfs_dentry_release(dentry);
	return node;
}

static vfs_dentry_t *vfs_get_dentry_at_recur(vfs_dentry_t *at, const char *path, long flags, long *loop_max, mode_t mode) {
	// we are going to modify it
	char new_path[strlen(path) + 1];
	strcpy(new_path, path);

	// first parse the path
	int path_depth   = 0;
	char last_is_sep = 1;
	char *path_array[64]; // hardcoded maximum identation level

	for (int i = 0; new_path[i]; i++) {
		// only if it's a path separator
		if (new_path[i] == '/') {
			new_path[i] = '\0';
			last_is_sep = 1;
			continue;
		}

		if (last_is_sep) {
			path_array[path_depth] = &new_path[i];
			path_depth++;
			last_is_sep = 0;
		}
	}

	// we handle O_PARENT here
	if (flags & O_PARENT) {
		// do we have a parent ?
		if (path_depth < 1) {
			return NULL;
		}
		path_depth--;
	}

	vfs_dentry_t *current_entry = vfs_dentry_ref(at);
	int ret;
	int created = 0;
	for (int i = 0; i < path_depth; i++) {
		if (!current_entry) break;
		vfs_dentry_t *next_entry = vfs_lookup(current_entry, path_array[i]);
		if (IS_ERR(next_entry)) {
			ret = PTR2ERR(next_entry);
			// maybee we can create it
			if (ret != -ENOENT || i != path_depth - 1 || !(flags & O_CREAT)) {
				goto error;
			}
			if (!current_entry->inode->ops->create) {
				ret = -EINVAL;
				goto error;
			}
			ret = vfs_create_at(current_entry, path_array[i], mode);
			if (ret < 0) goto error;
			// we need to manually fetch the new entry
			next_entry = vfs_lookup(current_entry, path_array[i]);
			if (IS_ERR(next_entry)) {
				ret = PTR2ERR(next_entry);
				goto error;
			}
			created = 1;
		}
		vfs_dentry_release(current_entry);
		current_entry = next_entry;

		if ((flags & O_NOFOLLOW) && i == path_depth - 1) {
			// do not follow last component on O_NOFOLLOW
			continue;
		}

		// follow symlink
		while (current_entry && current_entry->inode->flags == VFS_LINK) {
			if (*loop_max <= 0) goto error;
			(*loop_max)--;
			vfs_node_t *symlink = current_entry->inode;
			char target[PATH_MAX];
			ssize_t size;
			if ((size = vfs_readlink(symlink, target, sizeof(target))) < 0) goto error;
			target[size]     = '\0';
			vfs_dentry_t *at = target[0] == '/' ? root : current_entry->parent;
			kassert(at);
			next_entry = vfs_get_dentry_at_recur(at, target, flags, loop_max, mode);
			if (IS_ERR(next_entry)) {
				ret = PTR2ERR(next_entry);
				goto error;
			}
			vfs_dentry_release(current_entry);
			current_entry = next_entry;
		}
	}

	// at this point an error happend or the current entry is valid
	kassert(current_entry);

	if (!created && (flags & O_EXCL)) {
		ret = -EEXIST;
		return NULL;
	}

	return current_entry;
error:
	vfs_dentry_release(current_entry);
	return ERR2PTR(ret);
}

vfs_dentry_t *vfs_get_dentry_at(vfs_dentry_t *at, const char *path, long flags, ...) {
	long loop_max = SYMLOOP_MAX;
	mode_t mode   = 0777;
	if (flags & O_CREAT) {
		va_list args;
		va_start(args, flags);
		mode = va_arg(args, mode_t);
		va_end(args);
	}

	if (!at) {
		// if absolute relative to root else relative to cwd
		if (path[0] == '/' || path[0] == '\0') {
			return vfs_get_dentry_at(root, path, flags, mode);
		}
		return vfs_get_dentry_at(get_current_proc()->cwd, path, flags, mode);
	}
	return vfs_get_dentry_at_recur(at, path, flags, &loop_max, mode);
}

vfs_fd_t *vfs_open_at(vfs_dentry_t *at, const char *path, long flags, ...) {
	mode_t mode = 0777;
	if (flags & O_CREAT) {
		va_list args;
		va_start(args, flags);
		mode = va_arg(args, mode_t);
		va_end(args);
	}

	vfs_dentry_t *dentry = vfs_get_dentry_at(at, path, flags, mode);
	if (IS_ERR(dentry)) return (vfs_fd_t *)dentry;

	vfs_fd_t *fd = vfs_open_node(dentry->inode, dentry, flags);
	vfs_dentry_release(dentry);
	return fd;
}

vfs_fd_t *vfs_alloc_fd(void) {
	return slab_alloc(&fd_slab);
}

vfs_fd_t *vfs_open_node(vfs_node_t *node, vfs_dentry_t *dentry, long flags) {
	// permission checking
	mode_t required_perm = PERM_READ;
	if (flags & O_RDWR) {
		required_perm = PERM_READ | PERM_WRITE;
	} else if (flags & O_WRONLY) {
		required_perm = PERM_WRITE;
	}
	if ((vfs_perm(node) & required_perm) != required_perm) {
		return ERR2PTR(-EACCES);
	}

	vfs_fd_t *fd = vfs_alloc_fd();
	if (!fd) return ERR2PTR(-ENOMEM);
	struct stat st;
	vfs_getattr(node, &st);

	fd->ops     = NULL;
	fd->private = node->private_inode;
	fd->inode   = vfs_node_ref(node);
	fd->dentry  = vfs_dentry_ref(dentry);
	fd->flags   = flags;
	fd->type    = node->flags;

	// call inode specific open before fd specific open
	if (node->ops && node->ops->open) {
		node->ops->open(fd);
	}

	int ret = 0;
	if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)) {
		device_t *device = device_from_number(st.st_rdev);
		if (!device) {
			ret = -ENODEV;
			goto error;
		}
		fd->ops     = device->ops;
		fd->private = device;
	}

	if (fd->ops && fd->ops->open) {
		ret = fd->ops->open(fd);
		if (ret < 0) {
error:
			vfs_node_release(fd->inode);
			slab_free(fd);
			return ERR2PTR(ret);
		}
	}


	/// update modify / access time
	if (flags & O_RDWR) {
		vfs_update_time(node, VNODE_ATTR_ATIME | VNODE_ATTR_MTIME);
	} else if (flags & O_WRONLY) {
		vfs_update_time(node, VNODE_ATTR_MTIME);
	} else {
		vfs_update_time(node, VNODE_ATTR_ATIME);
	}

	return fd;
}

void vfs_close(vfs_fd_t *fd) {
	if (!fd) return;
	if (ref_count_dec(&fd->ref_count) > 1) {
		return;
	}

	if (fd->ops && fd->ops->close) {
		fd->ops->close(fd);
	}

	if (fd->type == VFS_BLOCK || fd->type == VFS_CHAR) {
		device_release(fd->private);
	}
	vfs_node_release(fd->inode);
	vfs_dentry_release(fd->dentry);

	slab_free(fd);
}

int vfs_user_perm(vfs_node_t *node, uid_t uid, gid_t gid) {
	struct stat st;
	vfs_getattr(node, &st);

	int is_other = 1;
	int perm     = 0;
	if (uid == 0) {
		// root can read/write anything
		perm |= 06;
		// root can search in any dir
		if (S_ISDIR(st.st_mode)) {
			perm |= 01;
		}
	}

	if (uid == st.st_uid) {
		is_other = 0;
		perm |= (st.st_mode >> 6) & 07;
	}
	if (gid == st.st_gid) {
		is_other = 0;
		perm |= (st.st_mode >> 3) & 07;
	}
	if (is_other) {
		perm |= st.st_mode & 07;
	}

	return perm;
}

int vfs_perm(vfs_node_t *node) {
	return vfs_user_perm(node, get_current_euid(), get_current_egid());
}

char *vfs_dentry_path(vfs_dentry_t *dentry) {
	if (!dentry) {
		goto unreachable;
	}
	if (dentry->flags & VFS_DENTRY_UNLINKED) {
		return strdup("(deleted)");
	}

	// TODO : use a dynamic buffer
	char path[PATH_MAX];
	size_t i  = PATH_MAX;
	path[--i] = '\0';
	while (dentry && dentry != root) {
		size_t len = strlen(dentry->name);
		i -= len;
		memcpy(path + i, dentry->name, len);
		path[--i] = '/';
		dentry    = dentry->parent;
	}
	if (dentry != root) {
unreachable:
		return strdup("(unreachable)");
	}
	if (!path[i]) {
		// we do not want root to appear as an empty string
		return strdup("/");
	}
	return strdup(path + i);
}
