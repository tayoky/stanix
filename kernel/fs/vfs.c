#include <kernel/vfs.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/vmm.h>
#include <kernel/time.h>
#include <kernel/list.h>
#include <kernel/device.h>
#include <kernel/slab.h>
#include <kernel/assert.h>
#include <limits.h>
#include <stddef.h>
#include <errno.h>
#include <poll.h>

//TODO : make this process specific
vfs_dentry_t *root;

static list_t fs_types;
static list_t superblocks;
static list_t dentries_lru;
static slab_cache_t dentries_slab;

static void vfs_remove_dentry(vfs_dentry_t *dentry) {
	if (!dentry->parent) return;
	list_remove(&dentry->parent->children, &dentry->children_node);
	dentry->parent = NULL;
}

static void vfs_just_cleanup_dentry(vfs_dentry_t *dentry) {
	vfs_close_node(dentry->inode);
	vfs_remove_dentry(dentry);
}

static int dentry_constructor(slab_cache_t *cache, void *data) {
	(void)cache;
	vfs_dentry_t *dentry = data;
	memset(dentry, 0, sizeof(vfs_dentry_t));
	return 0;
}

static void *dentry_evict(slab_cache_t *cache) {
	(void)cache;
	if (!dentries_lru.first_node) return NULL;
	vfs_dentry_t *dentry = container_of(dentries_lru.first_node, vfs_dentry_t, lru_node);
	list_remove(&dentries_lru, &dentry->lru_node);
	vfs_dentry_t *parent = dentry->parent;
	vfs_just_cleanup_dentry(dentry);
	vfs_release_dentry(parent);
}

static void dentry_add_lru(vfs_dentry_t *dentry) {
	list_append(&dentries_lru, &dentry->lru_node);
}

void init_vfs(void) {
	kstatusf("init vfs... ");
	slab_init(&dentries_slab, sizeof(vfs_dentry_t), "vfs dentries");
	dentries_slab.constructor = dentry_constructor;
	dentries_slab.evict       = dentry_evict;

	root = slab_alloc(&dentries_slab);
	root->ref_count = 1;
	strcpy(root->name, "[root]");

	init_list(&fs_types);
	init_list(&superblocks);
	kok();
}

void vfs_register_fs(vfs_filesystem_t *fs) {
	list_append(&fs_types, &fs->node);
}

void vfs_unregister_fs(vfs_filesystem_t *fs) {
	list_remove(&fs_types, &fs->node);
}

//basename without modyfing anything
static const char *vfs_basename(const char *path) {
	const char *base = path + strlen(path) - 1;
	//path of directory might finish with '/' like /tmp/dir/
	if (*base == '/')base--;
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
	if (!parent) {
		return -ENONET;
	}

	if (parent->inode->flags != VFS_DIR) {
		vfs_release_dentry(parent);
		return -ENOTDIR;
	}

	vfs_dentry_t *dentry = slab_alloc(&dentries_slab);
	if (!dentry) {
		vfs_release_dentry(parent);
		return -ENOMEM;
	}
	strcpy(dentry->name, vfs_basename(path));
	dentry->ref_count = 0;

	*_parent = parent;
	*_dentry = dentry;
	
	return 0;
}

static void vfs_add_dentry(vfs_dentry_t *parent, vfs_dentry_t *child) {
	// child hold a ref to the parent
	child->parent = vfs_dup_dentry(parent);
	list_append(&parent->children, &child->children_node);
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
		vfs_close_node(superblock->root);
		kfree(superblock);
	}
}

int vfs_auto_mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data) {
	vfs_superblock_t *superblock = NULL;
	foreach(node, &fs_types) {
		vfs_filesystem_t *fs = (vfs_filesystem_t *)node;
		if (!strcmp(fs->name, filesystemtype)) {
			if (!fs->mount) {
				return -ENODEV;
			}
			int ret = fs->mount(source, target, mountflags, data, &superblock);
			if (ret < 0) return ret;
			if (!superblock) return ret;

			// mount the superblock
			ret = vfs_mount(target, superblock);
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
	root_dentry->parent = vfs_dup_dentry(mount_point->parent);
	memcpy(root_dentry->name, mount_point->name, sizeof(mount_point->name));
	root_dentry->inode     = vfs_dup_node(superblock->root);
	root_dentry->ref_count = 1;
	root_dentry->flags     = VFS_DENTRY_MOUNT;

	// make a new ref to the mount point to prevent it from being close
	root_dentry->old = vfs_dup_dentry(mount_point);

	// insert the new fake dentry at the place of the original one
	if (mount_point->parent) {
		list_remove(&mount_point->parent->children, &mount_point->children_node);
		list_append(&mount_point->parent->children, &root_dentry->children_node);
	} else if(mount_point == root) {
		// special case for root
		root = root_dentry;
	}
	return 0;
}

int vfs_mountat(vfs_dentry_t *at, const char *name, vfs_superblock_t *superblock) {
	// first open the mount point
	vfs_dentry_t *mount_point = vfs_get_dentry_at(at, name, O_RDWR);
	if (!mount_point) {
		return -ENOENT;
	}

	int ret = vfs_mount_on(mount_point, superblock);

	vfs_release_dentry(mount_point);

	return ret;
}

int vfs_mount(const char *name, vfs_superblock_t *superblock) {
	return vfs_mountat(NULL, name, superblock);
}

int vfs_unmount(const char *path) {
	return vfs_unmountat(NULL, path);
}

int vfs_unmountat(vfs_dentry_t *at, const char *path) {
	vfs_dentry_t *mount_point = vfs_get_dentry_at(at, path, 0);
	if (!mount_point) {
		return -ENOENT;
	}

	if (!(mount_point->flags & VFS_DENTRY_MOUNT)) {
		// not even a mount point
		vfs_release_dentry(mount_point);
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

	vfs_release_dentry(mount_point);
	return 0;
}

ssize_t vfs_read(vfs_fd_t *fd, void *buffer, uint64_t offset, size_t count) {
	if (fd->type == VFS_BLOCK || fd->type == VFS_CHAR) {
		// check if device is unpluged
		if (((device_t *)fd->private)->type == DEVICE_UNPLUGED) return -ENODEV;
	} else if (fd->type & VFS_DIR) {
		return -EISDIR;
	}
	if (fd->flags & O_WRONLY) {
		return -EBADF;
	}

	if (fd->ops->read) {
		return fd->ops->read(fd, buffer, offset, count);
	} else {
		return -EINVAL;
	}
}

ssize_t vfs_write(vfs_fd_t *fd, const void *buffer, uint64_t offset, size_t count) {
	if (fd->type == VFS_BLOCK || fd->type == VFS_CHAR) {
		// check if device is unpluged
		if (((device_t *)fd->private)->type == DEVICE_UNPLUGED) return -ENODEV;
	} else if (fd->type & VFS_DIR) {
		return -EISDIR;
	}
	if (!(fd->flags & (O_WRONLY | O_RDWR))) {
		return -EBADF;
	}
	if (fd->ops->write) {
		return fd->ops->write(fd, buffer, offset, count);
	} else {
		return -EINVAL;
	}
}

int vfs_ioctl(vfs_fd_t *fd, long request, void *arg) {
	if (!fd || !fd->ops->ioctl) return -EBADF;
	if (fd->type == VFS_BLOCK || fd->type == VFS_CHAR) {
		// check if device is unpluged
		if (((device_t *)fd->private)->type == DEVICE_UNPLUGED) return -ENODEV;
	}
	return fd->ops->ioctl(fd, request, arg);
}

ssize_t vfs_readlink(vfs_node_t *node, char *buf, size_t bufsiz) {
	if (node->flags != VFS_LINK) {
		return -ENOLINK;
	}
	if (node->ops->readlink) {
		return node->ops->readlink(node, buf, bufsiz);
	} else {
		return -EINVAL;
	}
}

int vfs_wait_check(vfs_fd_t *fd, short type) {
	if (fd->type == VFS_BLOCK || fd->type == VFS_CHAR) {
		// check if device is unpluged
		if (((device_t *)fd->private)->type == DEVICE_UNPLUGED) return type & POLLHUP;
	}
	if (fd->ops->wait_check) {
		return fd->ops->wait_check(fd, type);
	} else {
		//by default report as ready for all request actions
		//so that stuff such as files are alaways ready
		return type;
	}
}

int vfs_wait(vfs_fd_t *fd, short type) {
	if (fd->ops->wait) {
		return fd->ops->wait(fd, type);
	} else {
		//mmmm...
		//how we land here ??
		return -EINVAL;
	}
}

vfs_dentry_t *vfs_lookup(vfs_dentry_t *entry, const char *name) {
	// cannot do lookup on negative entry
	if (vfs_dentry_is_negative(entry)) {
		return NULL;
	}

	if (entry->inode->flags != VFS_DIR) {
		return NULL;
	}

	// handle .. here so we can handle the parent of mount point
	if ((!strcmp("..", name)) && entry->parent) {
		return vfs_dup_dentry(entry->parent);
	}

	if ((!strcmp(".", name))) {
		return vfs_dup_dentry(entry);
	}

	// first search in the dentries cache
	foreach(list_node, &entry->children) {
		vfs_dentry_t *current_entry = container_of(list_node, vfs_dentry_t, children_node);
		if (!strcmp(current_entry->name, name)) {
			// cached entries must not be negative
			kassert(!vfs_dentry_is_negative(current_entry));
			return vfs_dup_dentry(current_entry);
		}
	}


	// it isen't chached
	// ask the fs for it
	if (!entry->inode->ops->lookup) {
		return NULL;
	}

	vfs_dentry_t *child_entry = slab_alloc(&dentries_slab);
	strcpy(child_entry->name, name);
	child_entry->ref_count = 1;

	if (entry->inode->ops->lookup(entry->inode, child_entry, name) < 0) {
		slab_free(child_entry);
		return NULL;
	}

	// link it in the dentry cache
	vfs_add_dentry(entry, child_entry);
	return child_entry;
}

void vfs_release_dentry(vfs_dentry_t *dentry) {
	while (dentry) {
		dentry->ref_count--;
		if (dentry->ref_count > 0) {
			return;
		}

		if (vfs_dentry_is_negative(dentry) || (dentry->inode->superblock->flags & VFS_SUPERBLOCK_NO_DCACHE)) {
			vfs_dentry_t *parent = dentry->parent;
			if (parent == dentry) parent = NULL;
			vfs_just_cleanup_dentry(dentry);
			slab_free(dentry);
			dentry = parent;
			continue;
		}

		// we can cache
		dentry_add_lru(dentry);
		return;
	}
}

void vfs_close_node(vfs_node_t *node) {
	if (!node) return;
	node->ref_count--;

	if (node->ref_count > 0) {
		return;
	}

	// we can finaly close

	if (node->ops->cleanup) {
		node->ops->cleanup(node);
	}

	kfree(node);
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

	vfs_release_dentry(parent);
	return 0;
error:
	vfs_release_dentry(parent);
	vfs_release_dentry(dentry);
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

	vfs_release_dentry(parent);
	return 0;
error:
	vfs_release_dentry(parent);
	vfs_release_dentry(dentry);
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

	vfs_release_dentry(parent);
	return 0;
error:
	vfs_release_dentry(parent);
	vfs_release_dentry(dentry);
	return ret;
}

int vfs_link_at(vfs_dentry_t *old_at, const char *old_path, vfs_dentry_t *new_at, const char *new_path) {
	vfs_dentry_t *old_dentry = vfs_get_dentry_at(old_at, old_path, O_NOFOLLOW);
	if (!old_dentry) return -EINVAL;

	vfs_dentry_t *new_parent = NULL;
	vfs_dentry_t *new_dentry = NULL;
	int ret = vfs_create_dentry(new_at, new_path, &new_parent, &new_dentry);
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

	vfs_release_dentry(old_dentry);
	vfs_release_dentry(new_parent);
	return 0;
error:
	vfs_release_dentry(old_dentry);
	vfs_release_dentry(new_parent);
	vfs_release_dentry(new_dentry);
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

	return 0;
error:
	vfs_release_dentry(parent);
	vfs_release_dentry(dentry);
	return ret;
}

int vfs_unlink_at(vfs_dentry_t *at, const char *path) {
	vfs_dentry_t *dentry = vfs_get_dentry_at(at, path, O_NOFOLLOW);
	if (!dentry) {
		return -ENOENT;
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
	vfs_release_dentry(dentry);
	return ret;
}


int vfs_rmdir_at(vfs_dentry_t *at, const char *path) {
	vfs_dentry_t *dentry = vfs_get_dentry_at(at, path, 0);
	if (!dentry) {
		return -ENOENT;
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
		// as far as i know you cannot unlink root
		ret = -ENOENT;
		goto error;
	}

	// call unlink on the parent
	vfs_node_t *parent = parent_entry->inode;
	if (!parent->ops || !parent->ops->unlink) {
		ret = -EINVAL;
		goto error;
	}
	ret = parent->ops->rmdir(parent, dentry);
	if (ret < 0) goto error;
	
	vfs_unlink_dentry(dentry);

error:
	vfs_release_dentry(dentry);
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
	dirent->d_ino  = 1; //some programs want non NULL inode
	if (node->ops->readdir) {
		return node->ops->readdir(node, index, dirent);
	} else {
		return -EINVAL;
	}
}

int vfs_chmod(vfs_node_t *node, mode_t perm) {
	struct stat st;
	int ret = vfs_getattr(node, &st);
	if (ret < 0)return ret;
	st.st_mode = perm;
	return vfs_setattr(node, &st);
}

int vfs_chown(vfs_node_t *node, uid_t owner, gid_t group_owner) {
	struct stat st;
	int ret = vfs_getattr(node, &st);
	if (ret < 0)return ret;
	st.st_uid = owner;
	st.st_gid = group_owner;
	return vfs_setattr(node, &st);
}

int vfs_getattr(vfs_node_t *node, struct stat *st) {
	if (!node) return -EINVAL;
	memset(st, 0, sizeof(struct stat));
	st->st_nlink = 1; //in case a driver forgot to set :D
	st->st_mode  = 0744; //default mode
	//make sure we can actually sync
	if (node->ops && node->ops->getattr) {
		int ret = node->ops->getattr(node, st);
		if (ret < 0)return ret;
	}

	//file type to mode
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

int vfs_setattr(vfs_node_t *node, struct stat *st) {
	//make sure we can actually sync
	if (!node || !node->ops || !node->ops->setattr) {
		return -EINVAL; //should be another error ... but what ???
	}
	return node->ops->setattr(node, st);
}

int vfs_chroot(vfs_node_t *new_root) {
	root = new_root;
	return 0;
}

vfs_node_t *vfs_get_node_at(vfs_dentry_t *at, const char *path, long flags) {
	vfs_dentry_t *dentry = vfs_get_dentry_at(at, path, flags);
	if (!dentry) return NULL;
	vfs_node_t *node = vfs_dup_node(dentry->inode);
	vfs_release_dentry(dentry);
	return node;
}

static vfs_dentry_t *vfs_get_dentry_at_recur(vfs_dentry_t *at, const char *path, long flags, long *loop_max) {
	if (!at) {
		//if absolute relative to root else relative to cwd
		if (path[0] == '/' || path[0] == '\0') {
			return vfs_get_dentry_at(root, path, flags);
		}
		return vfs_get_dentry_at(get_current_proc()->cwd, path, flags);
	}

	//we are going to modify it
	char new_path[strlen(path) + 1];
	strcpy(new_path, path);

	//first parse the path
	int path_depth = 0;
	char last_is_sep = 1;
	char *path_array[64]; //hardcoded maximum identation level

	for (int i = 0; new_path[i]; i++) {
		//only if it's a path separator
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
		//do we have a parent ?
		if (path_depth < 1) {
			return NULL;
		}
		path_depth--;
	}

	vfs_dentry_t *current_entry = vfs_dup_dentry(at);

	for (int i = 0; i < path_depth; i++) {
		if (!current_entry) return NULL;

		vfs_dentry_t *next_entry = vfs_lookup(current_entry, path_array[i]);
		if (!next_entry) goto error;
		vfs_release_dentry(current_entry);
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
			target[size] = '\0';
			vfs_dentry_t *at = target[0] == '/' ? root : current_entry->parent;
			kassert(at);
			next_entry = vfs_get_dentry_at_recur(at, target, flags, loop_max);
			if (!next_entry) goto error;
			vfs_release_dentry(current_entry);
			current_entry = next_entry;
		}
	}

	if (!current_entry) return NULL;

	return current_entry;
error:
	vfs_release_dentry(current_entry);
	return NULL;
}

vfs_dentry_t *vfs_get_dentry_at(vfs_dentry_t *at, const char *path, long flags) {
	long loop_max = SYMLOOP_MAX;
	return vfs_get_dentry_at_recur(at, path, flags, &loop_max);
}

vfs_fd_t *vfs_open_at(vfs_dentry_t *at, const char *path, long flags) {
	vfs_dentry_t *dentry = vfs_get_dentry_at(at, path, flags);
	if (!dentry) return NULL;

	vfs_fd_t *fd = vfs_open_node(dentry->inode, dentry, flags);
	vfs_release_dentry(dentry);
	return fd;
}

vfs_fd_t *vfs_open_node(vfs_node_t *node, vfs_dentry_t *dentry, long flags) {
	struct stat st;
	vfs_fd_t *fd = kmalloc(sizeof(vfs_fd_t));
	memset(fd, 0, sizeof(vfs_fd_t));
	vfs_getattr(node, &st);

	fd->ops       = NULL;
	fd->private   = node->private_inode;
	fd->inode     = vfs_dup_node(node);
	fd->dentry    = vfs_dup_dentry(dentry);
	fd->flags     = flags;
	fd->ref_count = 1;
	fd->type      = node->flags;

	// call inode specific open before fd specific open
	if (node->ops && node->ops->open) {
		node->ops->open(fd);
	}

	if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)) {
		device_t *device = device_from_number(st.st_rdev);
		if (!device) goto error;
		fd->ops     = device->ops;
		fd->private = device;
	}

	if (fd->ops && fd->ops->open) {
		int ret = fd->ops->open(fd);
		if (ret < 0) {
		error:
			vfs_close_node(fd->inode);
			kfree(fd);
			return NULL;
		}
	}


	/// update modify / access time
	if (flags & O_RDWR) {
		st.st_mtime = NOW();
		st.st_atime = NOW();
	} else if (flags & O_WRONLY) {
		st.st_mtime = NOW();
	} else {
		st.st_atime = NOW();
	}
	vfs_setattr(node, &st);

	return fd;
}

void vfs_close(vfs_fd_t *fd) {
	if (!fd) return;
	fd->ref_count--;
	if (fd->ref_count > 0) return;
	vfs_close_node(fd->inode);

	if (fd->ops && fd->ops->close) {
		fd->ops->close(fd);
	}

	kfree(fd);
}

int vfs_user_perm(vfs_node_t *node, uid_t uid, gid_t gid) {
	struct stat st;
	vfs_getattr(node, &st);

	int is_other = 1;
	int perm = 0;
	if (uid == 0) {
		// root can read/write anything
		perm |= 06;
	}

	if (uid == st.st_uid) {
		is_other = 0;
		perm |= st.st_mode & 07;
	}
	if (gid == st.st_gid) {
		is_other = 0;
		perm |= (st.st_mode >> 3) & 07;
	}
	if (is_other) {
		perm |= (st.st_mode >> 6) & 07;
	}

	return perm;
}

int vfs_perm(vfs_node_t *node) {
	return vfs_user_perm(node, get_current_proc()->euid, get_current_proc()->egid);
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
	size_t i = PATH_MAX;
	path[--i] = '\0';
	while (dentry && dentry != root) {
		size_t len = strlen(dentry->name);
		i -= len;
		memcpy(path + i, dentry->name, len);
		path[--i] = '/';
		dentry = dentry->parent;
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
