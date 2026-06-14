#include <kernel/assert.h>
#include <kernel/process.h>
#include <kernel/slab.h>
#include <kernel/string.h>
#include <kernel/vfs.h>

// TODO : make this process specific
vfs_dentry_t *root;

static list_t dentries_lru;
static slab_cache_t dentries_slab;

void vfs_dentry_add(vfs_dentry_t *parent, vfs_dentry_t *child) {
	// child hold a ref to the parent
	child->parent = vfs_dentry_ref(parent);
	list_append(&parent->children, &child->children_node);
}

void vfs_dentry_remove(vfs_dentry_t *dentry) {
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

void vfs_dentry_add_lru(vfs_dentry_t *dentry) {
	list_append(&dentries_lru, &dentry->lru_node);
}

void vfs_dentry_remove_lru(vfs_dentry_t *dentry) {
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
		vfs_dentry_add_lru(dentry);
		return;
	}
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

vfs_dentry_t *vfs_dentry_allocate(void) {
	return slab_alloc(&dentries_slab);
}

void vfs_set_root(vfs_dentry_t *dentry) {
    root = dentry;
}

vfs_dentry_t *vfs_get_root(void) {
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
