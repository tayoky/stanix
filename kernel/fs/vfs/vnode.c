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

	if (!S_ISDIR(parent->inode->mode)) {
		vfs_dentry_release(parent);
		return -ENOTDIR;
	}

	vfs_dentry_t *dentry = vfs_dentry_allocate();
	if (!dentry) {
		vfs_dentry_release(parent);
		return -ENOMEM;
	}

	vfs_node_acquire_write(parent->inode);

	vfs_dentry_t *exist = vfs_lookup(parent, vfs_basename(path));
	if ((IS_ERR(exist) && PTR2ERR(exist) != -ENOENT) || (!IS_ERR(exist) && exist)) {
		int ret = -EEXIST;
		if (IS_ERR(exist)) {
			ret = PTR2ERR(exist);
		} else {
			vfs_dentry_release(exist);
		}
		vfs_node_release_write(parent->inode);
		vfs_dentry_release(parent);
		vfs_dentry_release(dentry);
		return ret;
	}

	if (!(vfs_perm(parent->inode) & PERM_WRITE)) {
		vfs_node_release_write(parent->inode);
		vfs_dentry_release(parent);
		vfs_dentry_release(dentry);
		return -EACCES;
	}
	strcpy(dentry->name, vfs_basename(path));
	dentry->ref_count = 1;

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
}

vfs_node_t *vfs_node_allocate(vfs_superblock_t *superblock) {
	if (!superblock || !superblock->ops || !superblock->ops->allocate_inode) return NULL;
	vfs_node_t *node = superblock->ops->allocate_inode(superblock);
	if (!node) return NULL;
	memset(node, 0, sizeof(vfs_node_t));
	node->flags |= VNODE_FLAG_NEW;
	node->ref_count = 1;
	return node;
}

vfs_node_t *vfs_node_get(vfs_superblock_t *superblock, ino_t inode_number) {
	rcu_acquire_read(&superblock->inodes.rcu);
	vfs_node_t *node = xarray_get(&superblock->inodes, inode_number);
	if (node) return node;
	rcu_release_read(&superblock->inodes.rcu);

	// the node is not cached, create it
	node = vfs_node_allocate(superblock);
	if (!node) return NULL;

	// FIXME RACE : fix when we get xarray_raw_cmpxchg
	vfs_node_t *old_node = xarray_cmpxchg(&superblock->inodes, inode_number, NULL, node);
	if (old_node) {
		// we raced
		vfs_node_ref(old_node);
		vfs_node_release(node);
		return old_node;
	}

	vfs_node_acquire_write(node);	
	return node;
}

int vfs_chmod(vfs_node_t *node, mode_t perm) {
	struct stat st;
	st.st_mode = perm;

	vfs_node_acquire_write(node);
	uid_t current_euid = get_current_euid();
	int ret;
	if (current_euid == node->uid || current_euid == EUID_ROOT) {
		ret = vfs_setattr(node, &st, VNODE_ATTR_MODE);
	} else {
		ret = -EPERM;
	}
	vfs_node_release_write(node);
	return ret;
}

int vfs_chown(vfs_node_t *node, uid_t owner, gid_t group_owner) {
	struct stat st;
	st.st_uid = owner;
	st.st_gid = group_owner;

	vfs_node_acquire_write(node);
	uid_t current_euid = get_current_euid();
	int ret;
	if (current_euid == node->uid || current_euid == EUID_ROOT) {
		// clear setuid bit and setgid bit
		st.st_mode = node->mode & ~(S_ISUID | S_ISGID);
		ret = vfs_setattr(node, &st, VNODE_ATTR_UID | VNODE_ATTR_GID);
	} else {
		ret = -EPERM;
	}
	vfs_node_release_write(node);
	return ret;
}

int vfs_utimes(vfs_node_t *node, const struct timeval times[2]) {
	struct stat st;
	st.st_atime = times[0].tv_sec;
	st.st_mtime = times[1].tv_sec;

	vfs_node_acquire_write(node);
	uid_t current_euid = get_current_euid();
	int ret;
	if (current_euid == node->uid || current_euid == EUID_ROOT) {
		ret = vfs_setattr(node, &st, VNODE_ATTR_ATIME | VNODE_ATTR_MTIME);
	} else {
		ret = -EPERM;
	}
	vfs_node_release_write(node);
	return ret;
}

int vfs_node_flush(vfs_node_t *node) {
	if (!node) return -EBADF;
	if (!(atomic_fetch_and(&node->flags, ~(VNODE_FLAG_DIRTY | VNODE_FLAG_DATA_DIRTY)) & (VNODE_FLAG_DIRTY | VNODE_FLAG_DATA_DIRTY))) {
		// not dirty
		return 0;
	}
	kassert(node->superblock);
	if (!node->superblock->ops || !node->superblock->ops->flush_inode) return 0;
	vfs_node_acquire_read(node);
	int ret = node->superblock->ops->flush_inode(node->superblock, node);
	vfs_node_release_read(node);
	return ret;
}

ssize_t vfs_readlink(vfs_node_t *node, char *buf, size_t bufsiz) {
	if (!S_ISLNK(node->mode)) {
		return -ENOLINK;
	}
	if (node->ops->readlink) {
		vfs_update_time(node, VNODE_ATTR_ATIME);
		return node->ops->readlink(node, buf, bufsiz);
	} else {
		return -EOPNOTSUPP;
	}
}

// FIXME RACE : we have a few races in there
// - 1 double lookup if two thread call ops->lookup on the same name
// - 2 dentry eviction before we get time to remove it from lru
// - 3 if a dentry is added to the cache between when we check the rculist and when call ops->lookup
vfs_dentry_t *vfs_lookup(vfs_dentry_t *entry, const char *name) {
	// cannot do lookup on negative entry
	if (vfs_dentry_is_negative(entry)) {
		return ERR2PTR(-EINVAL);
	}

	if (!S_ISDIR(entry->inode->mode)) {
		return ERR2PTR(-ENOTDIR);
	}

	vfs_node_acquire_read(entry->inode);

	// check perm
	int ret = 0;
	if (!(vfs_perm(entry->inode) & PERM_EXECUTE)) {
		ret = -EACCES;
error:
		vfs_node_release_read(entry->inode);
		return ERR2PTR(ret);
	}

	// handle .. here so we can handle the parent of mount point
	if ((!strcmp("..", name)) && entry->parent) {
		vfs_node_release_read(entry->inode);
		return vfs_dentry_ref(entry->parent);
	}

	if ((!strcmp(".", name))) {
		vfs_node_release_read(entry->inode);
		return vfs_dentry_ref(entry);
	}

	// first search in the dentries cache
	rculist_acquire_read(&entry->children);
	rculist_foreach (list_node, &entry->children) {
		vfs_dentry_t *current_entry = container_of(list_node, vfs_dentry_t, children_node);
		if (!strcmp(current_entry->name, name)) {
			// cached entries must not be negative
			kassert(!vfs_dentry_is_negative(current_entry));

			// follow mount points
			current_entry = vfs_dentry_follow_mount_points(current_entry);
			vfs_dentry_ref(current_entry);

			// we might need to remove it from lru
			if (current_entry->ref_count == 1) {
				vfs_dentry_remove_lru(current_entry);
			}
			rculist_release_read(&entry->children);
			vfs_node_release_read(entry->inode);
			return current_entry;
		}
	}
	rculist_release_read(&entry->children);

	// it isen't chached
	// ask the fs for it
	if (!entry->inode->ops->lookup) {
		ret = -EOPNOTSUPP;
		goto error;
	}

	vfs_dentry_t *child_entry = vfs_dentry_allocate();
	strcpy(child_entry->name, name);
	child_entry->ref_count = 1;

	ret = entry->inode->ops->lookup(entry->inode, child_entry);
	if (ret < 0) {
		slab_free(child_entry);
		goto error;
	}
	kassert(!vfs_dentry_is_negative(child_entry));

	// no need to follow mount points we just loaded it
	// it cannot be mounted yet

	// link it in the dentry cache
	vfs_dentry_add(entry, child_entry);
	vfs_node_release_read(entry->inode);
	return child_entry;
}

void vfs_node_release(vfs_node_t *node) {
	if (!node) return;

	if (ref_count_dec(&node->ref_count) > 1) {
		return;
	}

	vfs_node_flush(node);

	// remove from the cache
	// FIXME : we have a race if someone get a new ref while le we clear
	xarray_clear(&node->superblock->inodes, node->number);

	// we can cleanup
	if (node->ops->cleanup) {
		node->ops->cleanup(node);
	} else {
		kfree(node);
	}
}

void vfs_node_mark_dirty(vfs_node_t *node) {
	atomic_fetch_or(&node->flags, VNODE_FLAG_DIRTY);
}

void vfs_node_mark_data_dirty(vfs_node_t *node) {
	atomic_fetch_or(&node->flags, VNODE_FLAG_DATA_DIRTY);
}

int vfs_create_at(vfs_dentry_t *at, const char *path, mode_t mode) {
	vfs_dentry_t *parent;
	vfs_dentry_t *dentry;
	int ret = vfs_create_dentry(at, path, &parent, &dentry);
	if (ret < 0) return ret;

	if (!parent->inode->ops || !parent->inode->ops->create) {
		ret = -EOPNOTSUPP;
		goto error;
	}
	ret = parent->inode->ops->create(parent->inode, dentry, mode);
	if (ret < 0) goto error;

	// now we can link the dentry if the fs filled it
	if (!vfs_dentry_is_negative(dentry)) {
		vfs_dentry_add(parent, dentry);
	}

error:
	vfs_node_release_write(parent->inode);
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
		ret = -EOPNOTSUPP;
		goto error;
	}
	ret = parent->inode->ops->mkdir(parent->inode, dentry, mode);
	if (ret < 0) goto error;

	// now we can link the dentry if the fs filled it
	if (!vfs_dentry_is_negative(dentry)) {
		vfs_dentry_add(parent, dentry);
	}

error:
	vfs_node_release_write(parent->inode);
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
		ret = -EOPNOTSUPP;
		goto error;
	}
	ret = parent->inode->ops->mknod(parent->inode, dentry, mode, dev);
	if (ret < 0) goto error;

	// now we can link the dentry if the fs filled it
	if (!vfs_dentry_is_negative(dentry)) {
		vfs_dentry_add(parent, dentry);
	}

error:
	vfs_node_release_write(parent->inode);
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
		ret = -EOPNOTSUPP;
		goto error;
	}
	ret = new_parent->inode->ops->link(old_dentry, new_parent->inode, new_dentry);
	if (ret < 0) goto error;

	// now we can link the dentry if the fs filled it
	if (!vfs_dentry_is_negative(new_dentry)) {
		vfs_dentry_add(new_parent, new_dentry);
	}

error:
	vfs_node_release_write(new_parent->inode);
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
		ret = -EOPNOTSUPP;
		goto error;
	}
	ret = parent->inode->ops->symlink(parent->inode, dentry, target);
	if (ret < 0) goto error;

error:
	vfs_node_release_write(parent->inode);
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
		ret = -EOPNOTSUPP;
		goto error;
	}
	ret = new_parent->inode->ops->rename(old_parent->inode, old_dentry, new_parent->inode, new_dentry, flags);
	if (ret < 0) goto error;

	// now we can link the dentry if the fs filled it
	if (!vfs_dentry_is_negative(new_dentry)) {
		vfs_dentry_add(new_parent, new_dentry);
	}

error:
	vfs_node_release_write(new_parent->inode);
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

	if (S_ISDIR(dentry->inode->mode)) {
		ret = -EISDIR;
		goto error;
	}

	vfs_dentry_t *parent_entry = dentry->parent;
	if (!parent_entry) {
		// as far as i know you cannot unlink root
		ret = -EINVAL;
		goto error;
	}

	vfs_node_acquire_write(parent_entry->inode);

	// cannot unlink mount points
	if (dentry->mount_point || dentry->shadow_mount_point) {
		vfs_node_release_write(parent_entry->inode);
		ret = -EBUSY;
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
			vfs_node_release_write(parent_entry->inode);
			ret = -EACCES;
			goto error;
		}

	} else {
		if (!(vfs_perm(parent_entry->inode) & PERM_WRITE)) {
			vfs_node_release_write(parent_entry->inode);
			ret = -EACCES;
			goto error;
		}
	}

	// call unlink on the parent
	vfs_node_t *parent = parent_entry->inode;
	if (!parent->ops || !parent->ops->unlink) {
		vfs_node_release_write(parent_entry->inode);
		ret = -EOPNOTSUPP;
		goto error;
	}
	ret = parent->ops->unlink(parent, dentry);
	vfs_node_release_write(parent_entry->inode);
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

	if (!S_ISDIR(dentry->inode->mode)) {
		ret = -ENOTDIR;
		goto error;
	}

	vfs_dentry_t *parent_entry = dentry->parent;
	if (!parent_entry) {
		// as far as i know you cannot rmdir root
		ret = -EINVAL;
		goto error;
	}

	vfs_node_acquire_write(parent_entry->inode);

	// cannot rmdir mount points
	if (dentry->mount_point || dentry->shadow_mount_point) {
		vfs_node_release_write(parent_entry->inode);
		ret = -EBUSY;
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
			vfs_node_release_write(parent_entry->inode);
			ret = -EACCES;
			goto error;
		}

	} else {
		if (!(vfs_perm(parent_entry->inode) & PERM_WRITE)) {
			vfs_node_release_write(parent_entry->inode);
			ret = -EACCES;
			goto error;
		}
	}

	// call rmdir on the parent
	vfs_node_t *parent = parent_entry->inode;
	if (!parent->ops || !parent->ops->rmdir) {
		vfs_node_release_write(parent_entry->inode);
		ret = -EOPNOTSUPP;
		goto error;
	}
	ret = parent->ops->rmdir(parent, dentry);
	vfs_node_release_write(parent_entry->inode);
	if (ret < 0) goto error;

	vfs_unlink_dentry(dentry);

error:
	vfs_dentry_release(dentry);
	return ret;
}

int vfs_readdir(vfs_node_t *node, unsigned long index, struct dirent *dirent) {
	if (!node) return -EBADF;
	if (!S_ISDIR(node->mode)) {
		return -ENOTDIR;
	}
	dirent->d_type = DT_UNKNOWN;
	dirent->d_ino  = 1; // some programs want non NULL inode
	if (node->ops->readdir) {
		vfs_update_time(node, VNODE_ATTR_ATIME);
		return node->ops->readdir(node, index, dirent);
	} else {
		return -EOPNOTSUPP;
	}
}

int vfs_getattr(vfs_node_t *node, struct stat *st) {
	if (!node) return -EINVAL;
	memset(st, 0, sizeof(struct stat));
	st->st_nlink = 1; // in case a driver forgot to set :D
	st->st_ino   = node->number;
	st->st_mode  = atomic_load(&node->mode);
	st->st_uid   = atomic_load(&node->uid);
	st->st_gid   = atomic_load(&node->gid);
	st->st_atime = atomic_load(&node->atime);
	st->st_mtime = atomic_load(&node->mtime);
	st->st_ctime = atomic_load(&node->ctime);

	// maybee the fs has a custom getattr
	if (node->ops && node->ops->getattr) {
		int ret = node->ops->getattr(node, st);
		if (ret < 0) return ret;
	}

	return 0;
}

static int vfs_raw_setattr(vfs_node_t *node, struct stat *st, int mask) {
	// make sure we can actually setattr
	if (!node) return -EBADF;
	spinlock_assert_acquired(&node->lock);
	if (!node->ops || !node->ops->setattr) {
		return -EOPNOTSUPP;
	}
	int ret = node->ops->setattr(node, st, mask);
	if (ret < 0) return ret;
	if (mask & VNODE_ATTR_MODE) atomic_store(&node->mode, (st->st_mode & ~S_IFMT) | (node->mode & S_IFMT));
	if (mask & VNODE_ATTR_UID) atomic_store(&node->uid, st->st_uid);
	if (mask & VNODE_ATTR_GID) atomic_store(&node->gid, st->st_gid);
	if (mask & VNODE_ATTR_ATIME) atomic_store(&node->atime, st->st_atime);
	if (mask & VNODE_ATTR_MTIME) atomic_store(&node->mtime, st->st_mtime);
	if (mask & VNODE_ATTR_CTIME) atomic_store(&node->ctime, st->st_ctime);
	vfs_node_mark_dirty(node);
	return ret;
}

int vfs_setattr(vfs_node_t *node, struct stat *st, int mask) {
	if (!node) return -EBADF;
	spinlock_acquire(&node->lock);
	int ret = vfs_raw_setattr(node, st, mask);
	spinlock_release(&node->lock);
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
