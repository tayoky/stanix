#include <kernel/assert.h>
#include <kernel/device.h>
#include <kernel/list.h>
#include <kernel/panic.h>
#include <kernel/print.h>
#include <kernel/process.h>
#include <kernel/refcount.h>
#include <kernel/slab.h>
#include <kernel/string.h>
#include <kernel/vfs.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stddef.h>


static list_t fs_types;
static list_t superblocks;

void init_vfs(void) {
	kstatusf("init vfs... ");
	init_vfs_fd();
	init_vfs_dentry();
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
				vfs_superblock_destroy(superblock);
			}
			return ret;
		}
	}

	return -ENODEV;
}

int vfs_mount_on(vfs_dentry_t *mount_point, vfs_superblock_t *superblock) {
	kdebugf("mount superblock on %s\n", mount_point->name);

	// create a new fake dentry for the root of the superblock
	vfs_dentry_t *root_dentry = vfs_dentry_allocate();
	root_dentry->parent       = mount_point->parent;
	memcpy(root_dentry->name, mount_point->name, sizeof(mount_point->name));
	root_dentry->inode     = vfs_node_ref(superblock->root);
	root_dentry->ref_count = 1;
	root_dentry->flags     = VFS_DENTRY_MOUNT;

	// make a new ref to the mount point to prevent it from being closed
	root_dentry->old = vfs_dentry_ref(mount_point);

	// insert the new fake dentry at the place of the original one
	if (mount_point->parent) {
		list_remove(&mount_point->parent->children, &mount_point->children_node);
		list_append(&mount_point->parent->children, &root_dentry->children_node);
	} else if (mount_point == vfs_get_root()) {
		// special case for root
		vfs_set_root(root_dentry);
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

	if (atomic_load(&mount_point->ref_count) > 1) {
		return -EBUSY;
	}

	vfs_superblock_destroy(mount_point->inode->superblock);

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
