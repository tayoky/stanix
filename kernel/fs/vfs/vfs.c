#include <kernel/assert.h>
#include <kernel/device.h>
#include <kernel/list.h>
#include <kernel/panic.h>
#include <kernel/print.h>
#include <kernel/process.h>
#include <kernel/refcount.h>
#include <kernel/slab.h>
#include <kernel/mutex.h>
#include <kernel/string.h>
#include <kernel/vfs.h>
#include <sys/mount.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stddef.h>

static slab_cache_t mount_points_slab;
static list_t fs_types;
static list_t superblocks;

// this lock is used to protect mounting on root
// since root does not have a parent to protect it
static mutex_t root_mount_lock;

void init_vfs(void) {
	kstatusf("init vfs... ");
	init_vfs_fd();
	init_vfs_dentry();
	slab_init(&mount_points_slab, sizeof(vfs_mount_point_t), "vfs-mount-points");
	list_init(&fs_types);
	list_init(&superblocks);
	kok();
}

void vfs_register_fs(vfs_filesystem_t *fs) {
	list_append(&fs_types, &fs->node);
}

int vfs_unregister_fs(vfs_filesystem_t *fs) {
	list_remove(&fs_types, &fs->node);
	// TODO : check if in use
	return 0;
}

int vfs_superblock_flush(vfs_superblock_t *superblock) {
	if (!superblock) return -EINVAL;
	// TODO : maybee use a dedicated dirty list
	rcu_acquire_read(&superblock->inodes.rcu);
	xarray_foreach (index, value, &superblock->inodes) {
		vfs_node_t *node = value;
		vfs_node_ref(node);
		rcu_release_read(&superblock->inodes.rcu);
		vfs_node_flush(node);
		vfs_node_release(node);
		rcu_acquire_read(&superblock->inodes.rcu);
	}
	rcu_release_read(&superblock->inodes.rcu);
	if (superblock->ops && superblock->ops->flush) {
		int ret = 0;
		if (ret < 0) return ret;
	}
	return 0;
}

static void vfs_superblock_destroy(vfs_superblock_t *superblock) {
	if (!superblock) return;
	vfs_superblock_flush(superblock);
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

	vfs_fd_t *src = source ? vfs_open(source, O_RDWR) : NULL;
	if (IS_ERR(src)) return PTR2ERR(src);

	vfs_filesystem_t *best = NULL;
	int best_score = 0;
	foreach (node, &fs_types) {
		vfs_filesystem_t *fs = container_of(node, vfs_filesystem_t, node);
		if (mountflags & MS_AUTO) {
			if (!fs->probe) continue;
			int score = fs->probe(src);
			if (score > best_score) {
				best = fs;
				best_score = score;
			}
		} else {
			if (!strcmp(fs->name, filesystemtype)) {
				best = fs;
				break;
			}
		}
	}


	if (!best || !best->mount) {
		vfs_close(src);
		return -ENODEV;
	}

	int ret = best->mount(src, target, mountflags, data, &superblock);
	if (ret < 0) {
		vfs_close(src);
		return ret;
	}
	if (!superblock) return ret;

	// mount the superblock
	ret                          = vfs_mount(target, mountflags, superblock);
	superblock->root->superblock = superblock;
	if (ret < 0) {
		vfs_superblock_destroy(superblock);
	}
	return ret;
}

static void vfs_dentry_acquire_mount_lock(vfs_dentry_t *dentry) {
	if (dentry->parent) {
		vfs_node_acquire_write(dentry->parent->inode);
	} else {
		mutex_acquire(&root_mount_lock);
	}
}

static void vfs_dentry_release_mount_lock(vfs_dentry_t *dentry) {
	if (dentry->parent) {
		vfs_node_release_write(dentry->parent->inode);
	} else {
		mutex_release(&root_mount_lock);
	}
}

int vfs_mount_on(vfs_dentry_t *mount_on, unsigned long flags, vfs_superblock_t *superblock) {
	// make sure to be on top of the mountpoint stack
	mount_on = vfs_dentry_follow_mount_points(mount_on);

	kdebugf("mount superblock on %s\n", mount_on->name);

	vfs_mount_point_t *mount_point = slab_alloc(&mount_points_slab);
	if (!mount_point) return -ENOMEM;

	kassert(!mount_on->shadow_mount_point);

	// create a new fake dentry for the root of the superblock
	vfs_dentry_t *root_dentry = vfs_dentry_allocate();
	root_dentry->parent       = vfs_dentry_ref(mount_on->parent);
	memcpy(root_dentry->name, mount_on->name, sizeof(mount_on->name));
	root_dentry->inode     = vfs_node_ref(superblock->root);
	root_dentry->ref_count = 0;
	root_dentry->mount_point = mount_point;
	root_dentry->flags       = VFS_DENTRY_MOUNT_POINT;

	// setup refs to prevent dentries from being released
	mount_point->shadow = vfs_dentry_ref(mount_on);
	mount_point->root   = vfs_dentry_ref(root_dentry);
	mount_point->flags  = flags;

	// update the old dentry
	mount_on->shadow_mount_point = mount_point;
	return 0;
}

int vfs_mount_at(vfs_dentry_t *at, const char *name, unsigned long flags, vfs_superblock_t *superblock) {
	// first open the mount point
	vfs_dentry_t *mount_point = vfs_get_dentry_at(at, name, O_RDWR);
	if (IS_ERR(mount_point)) return PTR2ERR(mount_point);
	vfs_dentry_acquire_mount_lock(mount_point);

	int ret = vfs_mount_on(mount_point, flags, superblock);

	vfs_dentry_release_mount_lock(mount_point);
	vfs_dentry_release(mount_point);
	return ret;
}

int vfs_unmount_at(vfs_dentry_t *at, const char *path) {
	vfs_dentry_t *root_dentry = vfs_get_dentry_at(at, path, 0);
	if (IS_ERR(root_dentry)) return PTR2ERR(root_dentry);
	vfs_dentry_acquire_mount_lock(root_dentry);

	int ret = 0;
	if (!(root_dentry->flags & VFS_DENTRY_MOUNT_POINT))  {
		// not even a mount point
		ret = -EINVAL;
		goto error;
	}
	vfs_mount_point_t *mount_point = root_dentry->mount_point;
	kassert(mount_point->root == root_dentry);

	if (atomic_load(&root_dentry->ref_count) > 2) {
		ret = -EBUSY;
		goto error;
	}

	// update the old entry
	vfs_dentry_t *mount_on = mount_point->shadow;
	mount_on->shadow_mount_point = NULL;

	vfs_dentry_release_mount_lock(root_dentry);

	// cleanup stuff
	vfs_superblock_t *superblock = root_dentry->inode->superblock;
	vfs_dentry_release(root_dentry);
	vfs_dentry_release(mount_point->root);
	vfs_dentry_release(mount_point->shadow);
	slab_free(mount_point);
	vfs_superblock_destroy(superblock);

	return 0;

error:
	vfs_dentry_release_mount_lock(root_dentry);
	vfs_dentry_release(root_dentry);
	return ret;
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
