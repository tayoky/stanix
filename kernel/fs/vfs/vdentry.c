#include <kernel/assert.h>
#include <kernel/process.h>
#include <kernel/slab.h>
#include <kernel/string.h>
#include <kernel/vfs.h>

// the absolute root
vfs_dentry_t *root;

static list_t dentries_lru;
static spinlock_t lru_lock;
static slab_cache_t dentries_slab;

void vfs_dentry_add(vfs_dentry_t *parent, vfs_dentry_t *child) {
	// child hold a ref to the parent
	atomic_store(&child->parent, vfs_dentry_ref(parent));
	rculist_append(&parent->children, &child->children_node);
}

void vfs_dentry_remove(vfs_dentry_t *dentry) {
	if (!dentry->parent) return;
	rculist_remove(&dentry->parent->children, &dentry->children_node);
	// child hold a ref to the parent
	vfs_dentry_release(dentry->parent);
	atomic_store(&dentry->parent, NULL);
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
	vfs_node_release(dentry->inode);
	vfs_dentry_remove(dentry);
	return 0;
}

static void *dentry_evict(slab_cache_t *cache) {
	(void)cache;
	if (!dentries_lru.first_node) return NULL;
	vfs_dentry_t *dentry = container_of(dentries_lru.first_node, vfs_dentry_t, lru_node);
	list_remove(&dentries_lru, &dentry->lru_node);
	return dentry;
}

static void vfs_dentry_add_lru(vfs_dentry_t *dentry) {
	spinlock_assert_acquired(&lru_lock);
	list_append(&dentries_lru, &dentry->lru_node);
}

static void vfs_dentry_remove_lru(vfs_dentry_t *dentry) {
	spinlock_assert_acquired(&lru_lock);
	list_remove(&dentries_lru, &dentry->lru_node);
}

void init_vfs_dentry(void) {
	slab_init(&dentries_slab, sizeof(vfs_dentry_t), "vfs-dentries");
	dentries_slab.constructor = dentry_constructor;
	dentries_slab.destructor  = dentry_destructor;
	dentries_slab.evict       = dentry_evict;

	root            = slab_alloc(&dentries_slab);
	root->ref_count = 1;
	strcpy(root->name, "[root]");
}

void vfs_dentry_release(vfs_dentry_t *dentry) {
	if (IS_ERR(dentry)) return;
	while (dentry) {
		if (ref_count_dec_if_not_one(&dentry->ref_count)) {
			return;
		}
		
		vfs_dentry_t *parent = dentry->parent;
		kassert(parent != dentry);

		if (parent) rculist_acquire_write(&parent->children);

		if (!vfs_dentry_is_populated(dentry) || (dentry->flags & VFS_DENTRY_FLAG_UNLINKED) || (dentry->inode->superblock->flags & VFS_SUPERBLOCK_NO_DCACHE)) {
			// we cannot cache
			if (ref_count_dec(&dentry->ref_count) > 1) {
				if (parent) rculist_release_write(&parent->children);
				return;
			}

			// we cannot use vfs_remove_entry cause it call vfs_dentry_release
			if (parent) {
				rculist_raw_remove(&parent->children, &dentry->children_node);
				rculist_release_write(&parent->children);
			}
			dentry->parent = NULL;
			slab_free(dentry);
			dentry = parent;
			continue;
		}

		if (ref_count_dec(&dentry->ref_count) == 1) {
			// this was the last ref, we can cache
			spinlock_acquire(&lru_lock);
			vfs_dentry_add_lru(dentry);
			spinlock_release(&lru_lock);
		}
		if (parent) rculist_release_write(&parent->children);
		return;
	}
}

vfs_dentry_t *vfs_dentry_follow_mount_points(vfs_dentry_t *dentry) {
	if (IS_ERR(dentry)) return dentry;
	while (dentry->shadow_mount_point) {
		dentry = dentry->shadow_mount_point->root;
		kassert(dentry);
	}
	return dentry;
}

static vfs_dentry_t *vfs_dentry_wait_ready_helper(vfs_dentry_t *entry) {
	cond_var_wait(&entry->flags, VFS_DENTRY_FLAG_NEW, 0);
	if (atomic_load(&entry->flags) & VFS_DENTRY_FLAG_ERROR) {
		int error = entry->error;
		vfs_dentry_release(entry);
		return ERR2PTR(error);
	} else {
		return entry;
	}
}

static vfs_dentry_t *vfs_internal_lookup(vfs_dentry_t *entry, const char *name) {
	rculist_foreach (list_node, &entry->children) {
		vfs_dentry_t *current_entry = container_of(list_node, vfs_dentry_t, children_node);
		if (!strcmp(current_entry->name, name)) {
			return current_entry;
		}
	}
	return NULL;
}

vfs_dentry_t *vfs_lookup(vfs_dentry_t *entry, const char *name) {
	// cannot do lookup on non populated entry
	if (!vfs_dentry_is_populated(entry)) {
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
	if (!strcmp("..", name)) {
		vfs_dentry_t *parent = atomic_load(&entry->parent);
		vfs_node_release_read(entry->inode);
		return vfs_dentry_ref(parent ? parent : entry);
	}

	if (!strcmp(".", name)) {
		vfs_node_release_read(entry->inode);
		return vfs_dentry_ref(entry);
	}

	// fast path : rcu search in the dentries cache
	rculist_acquire_read(&entry->children);
	vfs_dentry_t *child_entry = vfs_internal_lookup(entry, name);
	if (child_entry) {
		// follow mount points
		child_entry = vfs_dentry_follow_mount_points(child_entry);

		if (!vfs_dentry_ref_if_not_zero(child_entry)) {
			spinlock_acquire(&lru_lock);
			if (!vfs_dentry_ref_if_not_zero(child_entry)) {
				vfs_dentry_remove_lru(child_entry);
				vfs_dentry_ref(child_entry);
			}
			spinlock_release(&lru_lock);
		}
		
		rculist_release_read(&entry->children);
		vfs_node_release_read(entry->inode);
		return vfs_dentry_wait_ready_helper(child_entry);
	}
	rculist_release_read(&entry->children);

	// it isen't chached
	// ask the fs for it
	if (!entry->inode->ops->lookup) {
		ret = -EOPNOTSUPP;
		goto error;
	}

	child_entry = vfs_dentry_allocate();
	if (!child_entry) {
		ret = -ENOMEM;
		goto error;
	}
	strcpy(child_entry->name, name);
	child_entry->mount_point = entry->mount_point;
	child_entry->ref_count = 1;
	child_entry->flags |= VFS_DENTRY_FLAG_NEW;

	// link the dentry
	rculist_acquire_write(&entry->children);
	vfs_dentry_t *old_entry = vfs_internal_lookup(entry, name);
	if (old_entry) {
		old_entry = vfs_dentry_follow_mount_points(old_entry);
		if (!vfs_dentry_ref_if_not_zero(old_entry)) {
			spinlock_acquire(&lru_lock);
			if (!vfs_dentry_ref_if_not_zero(old_entry)) {
				vfs_dentry_remove_lru(old_entry);
				vfs_dentry_ref(old_entry);
			}
			spinlock_release(&lru_lock);
		}
		rculist_release_write(&entry->children);
		vfs_node_release_read(entry->inode);
		vfs_dentry_release(child_entry);
		return vfs_dentry_wait_ready_helper(old_entry);
	}
	child_entry->parent = vfs_dentry_ref(entry);
	rculist_raw_append(&entry->children, &child_entry->children_node);
	rculist_release_write(&entry->children);

	ret = entry->inode->ops->lookup(entry->inode, child_entry);
	if (ret < 0) {
		vfs_dentry_remove(child_entry);
		child_entry->error = ret;
		atomic_fetch_or(&child_entry->flags, VFS_DENTRY_FLAG_ERROR);
		atomic_fetch_and(&child_entry->flags, ~VFS_DENTRY_FLAG_NEW);
		cond_var_wakeup(&child_entry->flags);
		vfs_dentry_release(child_entry);
		goto error;
	}
	kassert(vfs_dentry_is_populated(child_entry));
	
	atomic_fetch_and(&child_entry->flags, ~VFS_DENTRY_FLAG_NEW);
	cond_var_wakeup(&child_entry->flags);

	vfs_node_release_read(entry->inode);

	// no need to follow mount points we just loaded it
	// it cannot be mounted yet
	return child_entry;
}

/**
 * @note consume a release to at
 */
static vfs_dentry_t *vfs_get_dentry_at_recur(vfs_dentry_t *at, const char *path, char last[NAME_MAX], long flags, long *loop_max, mode_t mode) {
	// we are going to modify it
	char new_path[strlen(path) + 1];
	strcpy(new_path, path);

	// first parse the path
	int path_depth   = 0;
	char last_is_sep = 1;
	char *path_array[128]; // hardcoded maximum identation level

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

	if (last) {
		// do we have a parent ?
		if (path_depth > 0) {
			snprintf(last, NAME_MAX, "%s", path_array[path_depth - 1]);
		} else {
			strcpy(last, "");
		}
		if (path_depth < 1) {
			return NULL;
		}
		path_depth--;
	}

	vfs_dentry_t *current_entry = at;
	int ret;
	int created = 0;
	for (int i = 0; i < path_depth; i++) {
		kassert(current_entry);
		vfs_dentry_t *next_entry = vfs_lookup(current_entry, path_array[i]);
		if (IS_ERR(next_entry)) {
			ret = PTR2ERR(next_entry);
			// maybe we can create it
			if (ret != -ENOENT || i != path_depth - 1 || !(flags & O_CREAT)) {
				goto error;
			}
			if (!current_entry->inode->ops->create) {
				ret = -EINVAL;
				goto error;
			}
			vfs_node_acquire_write(current_entry->inode);
			created = 1;
			ret = vfs_create_at(current_entry, path_array[i], mode);
			if (ret == -EEXIST) {
				// created in the meanwhile
				created = 0;
			} else if (ret < 0) {
				vfs_node_release_write(current_entry->inode);
				goto error;
			}

			// we need to manually fetch the new entry
			next_entry = vfs_lookup(current_entry, path_array[i]);

			vfs_node_release_write(current_entry->inode);
			if (IS_ERR(next_entry)) {
				ret = PTR2ERR(next_entry);
				goto error;
			}
		}
		vfs_dentry_release(current_entry);

		current_entry = next_entry;

		if ((flags & O_NOFOLLOW) && i == path_depth - 1) {
			// do not follow last component on O_NOFOLLOW
			continue;
		}

		// follow symlinks
		while (current_entry && S_ISLNK(current_entry->inode->mode)) {
			if (*loop_max <= 0) goto error;
			(*loop_max)--;
			vfs_node_t *symlink = current_entry->inode;
			char target[PATH_MAX];
			ssize_t size;
			if ((size = vfs_readlink(symlink, target, sizeof(target))) < 0) goto error;
			target[size]     = '\0';
			vfs_dentry_t *at = NULL;
			if (target[0] != '/') {
				at = vfs_dentry_ref(current_entry->parent);
				kassert(at);
			}
			next_entry = vfs_get_dentry_at_recur(at, target, NULL, flags, loop_max, mode);
			if (IS_ERR(next_entry)) {
				ret = PTR2ERR(next_entry);
				goto error;
			}
			vfs_dentry_release(current_entry);
			current_entry = next_entry;
		}
	}

	// at this point the current entry should be valid
	kassert(current_entry);

	if (!created && (flags & O_EXCL)) {
		ret = -EEXIST;
		goto error;
	}

	return current_entry;
error:
	vfs_dentry_release(current_entry);
	return ERR2PTR(ret);
}

vfs_dentry_t *vfs_get_dentry_parent_at(vfs_dentry_t *at, const char *path, char last[NAME_MAX], long flags, ...) {
	long loop_max = SYMLOOP_MAX;
	mode_t mode   = 0777;
	if (flags & O_CREAT) {
		va_list args;
		va_start(args, flags);
		mode = va_arg(args, mode_t);
		va_end(args);
	}

	if (at) {
		vfs_dentry_ref(at);
	} else {
		if (get_current_proc()) {
			// if absolute relative to root else relative to cwd
			if (path[0] == '/' || path[0] == '\0') {
				at = vfs_context_get_root(&get_current_proc()->vfs_context);
			} else {
				at = vfs_context_get_cwd(&get_current_proc()->vfs_context);
			}
		} else {
			at = vfs_dentry_ref(root);
		}
	}
	return vfs_get_dentry_at_recur(at, path, last, flags, &loop_max, mode);
}

vfs_dentry_t *vfs_dentry_allocate(void) {
	return slab_alloc(&dentries_slab);
}

void vfs_set_root(vfs_dentry_t *dentry) {
    root = dentry;
}

ATTR_RETURN_NONNULL vfs_dentry_t *vfs_get_root(void) {
    return root;
}

int vfs_chroot(vfs_dentry_t *new_root) {
	vfs_set_root(new_root);
	return 0;
}

char *vfs_dentry_path(vfs_dentry_t *dentry) {
	if (!dentry) {
		goto unreachable;
	}
	if (dentry->flags & VFS_DENTRY_FLAG_UNLINKED) {
		return strdup("(deleted)");
	}
	
	vfs_dentry_t *top;
	if (get_current_proc()) {
		top = vfs_context_get_root(&get_current_proc()->vfs_context);
		vfs_dentry_release(top);
	} else {
		top = root;
	}

	// TODO : use a dynamic buffer
	char path[PATH_MAX];
	size_t i  = PATH_MAX;
	path[--i] = '\0';
	while (dentry && dentry != top) {
		size_t len = strlen(dentry->name);
		i -= len;
		memcpy(path + i, dentry->name, len);
		path[--i] = '/';
		dentry    = dentry->parent;
	}
	if (dentry != top) {
unreachable:
		return strdup("(unreachable)");
	}
	if (!path[i]) {
		// we do not want root to appear as an empty string
		return strdup("/");
	}
	return strdup(path + i);
}
