#include <kernel/assert.h>
#include <kernel/process.h>
#include <kernel/slab.h>
#include <kernel/time.h>
#include <kernel/vfs.h>

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

	vfs_dentry_t *dentry = vfs_dentry_allocate();
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
	vfs_dentry_remove(dentry);
	dentry->flags |= VFS_DENTRY_UNLINKED;
}


void vfs_init_created_node(vfs_node_t *node) {
	node->uid   = get_current_euid();
	node->gid   = get_current_egid();
	node->atime = node->mtime = node->ctime = gettime_sec(CLOCK_REALTIME);
	node->ref_count                         = 1;
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
				vfs_dentry_remove_lru(ret);
			}
			return ret;
		}
	}


	// it isen't chached
	// ask the fs for it
	if (!entry->inode->ops->lookup) {
		return ERR2PTR(-EINVAL);
	}

	vfs_dentry_t *child_entry = vfs_dentry_allocate();
	strcpy(child_entry->name, name);
	child_entry->ref_count = 1;

	int ret = entry->inode->ops->lookup(entry->inode, child_entry);
	if (ret < 0) {
		slab_free(child_entry);
		return ERR2PTR(ret);
	}

	// link it in the dentry cache
	vfs_dentry_add(entry, child_entry);
	return child_entry;
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
		vfs_dentry_add(parent, dentry);
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
		vfs_dentry_add(parent, dentry);
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
		vfs_dentry_add(parent, dentry);
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
		vfs_dentry_add(new_parent, new_dentry);
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
		vfs_dentry_add(new_parent, new_dentry);
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
