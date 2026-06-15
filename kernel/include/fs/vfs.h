#ifndef KERNEL_VFS_H
#define KERNEL_VFS_H

#include <kernel/assert.h>
#include <kernel/atomic.h>
#include <kernel/list.h>
#include <kernel/refcount.h>
#include <kernel/spinlock.h>
#include <kernel/time.h>
#include <kernel/xarray.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>

// just in case we use a weird limits.h
#ifndef PATH_MAX
#define PATH_MAX 256
#endif

// we cannot include unistd.h
#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

struct vfs_node;
struct vmm_seg;
struct vfs_inode_ops;
struct vfs_fd_ops;
struct vfs_superblock_ops;
struct superblock;
struct poll_event;

/**
 * @brief represent an inode
 */
typedef struct vfs_node {
	struct vfs_superblock *superblock;
	struct vfs_inode_ops *ops;
	ino_t number;
	ATOMIC(long) flags;
	ref_count_t ref_count;
	ATOMIC(uid_t) uid;
	ATOMIC(gid_t) gid;
	ATOMIC(mode_t) mode;
	ATOMIC(time_t) atime;
	ATOMIC(time_t) mtime;
	ATOMIC(time_t) ctime;
	spinlock_t lock;
} vfs_node_t;

#define VNODE_FLAG_DIRTY 0x01

/**
 * @brief represent a directory entry
 */
typedef struct vfs_dentry {
	list_node_t children_node;
	list_node_t lru_node;
	char name[256];
	vfs_node_t *inode;
	ino_t inode_number;
	long flags;
	struct vfs_dentry *parent;
	struct vfs_dentry *old; // used for mount point
	list_t children;
	ref_count_t ref_count;
} vfs_dentry_t;

#define VFS_DENTRY_MOUNT    0x01 // the dentry is a mount point
#define VFS_DENTRY_UNLINKED 0x02 // the dentry is unlinked

/**
 * @brief represent an open context with a file/dir/device
 */
typedef struct vfs_fd {
	vfs_node_t *inode;
	vfs_dentry_t *dentry;
	struct vfs_fd_ops *ops;
	void *private;
	ref_count_t ref_count;
	long flags;
	long type;
	off_t offset;
} vfs_fd_t;

/**
 * @brief all operations that can be done on a \ref vfs_node_t
 */
typedef struct vfs_inode_ops {
	int (*lookup)(vfs_node_t *vnode, vfs_dentry_t *dentry);
	int (*readdir)(vfs_node_t *vnode, unsigned long index, struct dirent *);
	int (*create)(vfs_node_t *vnode, vfs_dentry_t *, mode_t perm);
	int (*mkdir)(vfs_node_t *vnode, vfs_dentry_t *, mode_t perm);
	int (*mknod)(vfs_node_t *vnode, vfs_dentry_t *, mode_t perm, dev_t dev);
	int (*unlink)(vfs_node_t *vnode, vfs_dentry_t *);
	int (*rmdir)(vfs_node_t *vnode, vfs_dentry_t *);
	int (*rename)(vfs_node_t *old_dir, vfs_dentry_t *old_dentry, vfs_node_t *new_dir, vfs_dentry_t *new_dentry, unsigned int flags);
	int (*link)(vfs_dentry_t *old_dentry, vfs_node_t *new_dir, vfs_dentry_t *new_dentry);
	int (*symlink)(vfs_node_t *vnode, vfs_dentry_t *, const char *target);
	ssize_t (*readlink)(vfs_node_t *vnode, char *, size_t);
	int (*setattr)(vfs_node_t *vnode, struct stat *, int mask);
	int (*getattr)(vfs_node_t *vnode, struct stat *);
	int (*truncate)(vfs_node_t *vnode, size_t);
	void (*cleanup)(vfs_node_t *);
	int (*open)(vfs_fd_t *);
} vfs_inode_ops_t;

/**
 * @brief contain all \ref vfs_fd_t operations
 */
typedef struct vfs_fd_ops {
	int (*open)(vfs_fd_t *);
	ssize_t (*read)(vfs_fd_t *, void *buf, off_t off, size_t count);
	ssize_t (*write)(vfs_fd_t *, const void *buf, off_t off, size_t count);
	int (*ioctl)(vfs_fd_t *, long, void *);
	int (*mmap)(vfs_fd_t *, off_t, struct vmm_seg *);
	void (*close)(vfs_fd_t *);
	off_t (*seek)(vfs_fd_t *, off_t offset, int whence);
	int (*poll_add)(vfs_fd_t *, struct poll_event *);
	int (*poll_remove)(vfs_fd_t *, struct poll_event *);
	int (*poll_get)(vfs_fd_t *, struct poll_event *);
} vfs_fd_ops_t;

typedef struct vfs_superblock {
	list_node_t node;
	xarray_t inodes;
	vfs_node_t *root;
	struct vfs_superblock_ops *ops;
	vfs_fd_t *device;
	long flags;
	char name[PATH_MAX];
} vfs_superblock_t;

typedef struct vfs_superblock_ops {
	void (*destroy)(vfs_superblock_t *superblock);
	int (*write_inode)(vfs_superblock_t *superblock, vfs_node_t *vnode);
	int (*read_inode)(vfs_superblock_t *superblock, vfs_node_t *vnode);
} vfs_superblock_ops_t;

#define VFS_SUPERBLOCK_NO_DCACHE 0x01

typedef struct vfs_filesystem {
	list_node_t node;
	char name[16];
	int (*mount)(const char *source, const char *target, unsigned long flags, const void *data, vfs_superblock_t **mount_point);
} vfs_filesystem_t;

void init_vfs(void);
void init_vfs_dentry(void);
void init_vfs_fd(void);

// inode operations

/**
 * @brief initalize a newly created inode with default owner, time, ...
 * @param node the inode to initialize
 */
void vfs_init_created_node(vfs_node_t *node);

int vfs_mount_on(vfs_dentry_t *mount_point, vfs_superblock_t *superblock);

/**
 * @brief mount a \ref vfs_superblock_t to the specified path
 * @param path the path to mount to
 * @param sueprblock the superblock to mount
 * @return 0 on success else error code
 */
static inline int vfs_mount(const char *name, vfs_superblock_t *superblock) {
	return vfs_mount_at(NULL, name, superblock);
}

static inline int vfs_unmount(const char *path) {
	return vfs_unmount_at(NULL, path);
}

int vfs_chroot(vfs_dentry_t *new_root);

int vfs_create_at(vfs_dentry_t *at, const char *path, mode_t mode);
int vfs_mkdir_at(vfs_dentry_t *at, const char *path, mode_t mode);
int vfs_mknod_at(vfs_dentry_t *at, const char *path, mode_t mode, dev_t dev);
int vfs_link_at(vfs_dentry_t *old_at, const char *old_path, vfs_dentry_t *new_at, const char *new_path);
int vfs_symlink_at(const char *target, vfs_dentry_t *at, const char *path);
int vfs_rename_at(vfs_dentry_t *old_at, const char *old_path, vfs_dentry_t *new_at, const char *new_path, unsigned int flags);
int vfs_unlink_at(vfs_dentry_t *at, const char *path);
int vfs_rmdir_at(vfs_dentry_t *at, const char *path);
int vfs_mount_at(vfs_dentry_t *at, const char *name, vfs_superblock_t *superblock);
int vfs_unmount_at(vfs_dentry_t *at, const char *path);

static inline int vfs_create(const char *path, mode_t mode) {
	return vfs_create_at(NULL, path, mode);
}

static inline int vfs_mkdir(const char *path, mode_t mode) {
	return vfs_mkdir_at(NULL, path, mode);
}

static inline int vfs_mknod(const char *path, mode_t mode, dev_t dev) {
	return vfs_mknod_at(NULL, path, mode, dev);
}

static inline vfs_link(const char *old_path, const char *new_path) {
	return vfs_link_at(NULL, old_path, NULL, new_path);
}

static inline int vfs_symlink(const char *target, const char *path) {
	return vfs_symlink_at(target, NULL, path);
}

static inline vfs_rename(const char *old_path, const char *new_path, unsigned int flags) {
	return vfs_rename_at(NULL, old_path, NULL, new_path, flags);
}

static inline vfs_unlink(const char *path) {
	return vfs_unlink_at(NULL, path);
}

static inline vfs_rmdir(const char *path) {
	return vfs_rmdir_at(NULL, path);
}

vfs_node_t *vfs_get_node_at(vfs_dentry_t *at, const char *pathname, long flags, ...);
static inline vfs_node_t *vfs_get_node(const char *pathname, long flags, ...) {
	mode_t mode = 0777;
	if (flags & O_CREAT) {
		va_list args;
		va_start(args, flags);
		mode = va_arg(args, mode_t);
		va_end(args);
	}
	return vfs_get_node_at(NULL, pathname, flags, mode);
}

static inline vfs_node_t *vfs_node_ref(vfs_node_t *node) {
	if (node) ref_count_inc(&node->ref_count);
	return node;
}

void vfs_node_release(vfs_node_t *node);

/**
 * @brief lock an inode for writing
 * @param node the inode to lock
 */
static inline vfs_node_lock(vfs_node_t *node) {
	if (node) spinlock_acquire(&node->lock);
}

/**
 * @brief unlock a inode previously locked with \ref vfs_node_lock
 * @param node the inode to unlock
 */
static inline vfs_node_unlock(vfs_node_t *node) {
	if (node) spinlock_release(&node->lock);
}

int vfs_getattr(vfs_node_t *node, struct stat *st);

/**
 * @brief set attributes on an inode
 * @param node the node to set the attribute of
 * @param st the value of the attributes
 * @param mask a mask of the atrributes to set
 * @return 0 on success, else error code
 * @note must be wrapped between \ref vfs_node_lock and \ref vfs_node_unlock
 */
int vfs_raw_setattr(vfs_node_t *node, struct stat *st, int mask);
#define VNODE_ATTR_MODE  0x01
#define VNODE_ATTR_UID   0x02
#define VNODE_ATTR_GID   0x04
#define VNODE_ATTR_ATIME 0x08
#define VNODE_ATTR_MTIME 0x10
#define VNODE_ATTR_CTIME 0x20

/**
 * @brief safely set attributes on an inode
 * @param node the node to set the attribute of
 * @param st the value of the attributes
 * @param mask a mask of the atrributes to set
 * @return 0 on success, else error code
 */
static inline int vfs_setattr(vfs_node_t *node, struct stat *st, int mask) {
	vfs_node_lock(node);
	int ret = vfs_raw_setattr(node, st, mask);
	vfs_node_unlock(node);
	return ret;
}

/**
 * @brief truncate a file to a specfied size
 * @param node context of the file
 * @param size the new size
 * @return 0 on success else error code
 */
static inline int vfs_truncate(vfs_node_t *node, size_t size) {
	if (!node) return -EBADF;
	if (!node->ops->truncate) return -EOPNOTSUPP;
	switch (node->mode & S_IFMT) {
	case S_IFREG:
		return node->ops->truncate(node, size);
	case S_IFDIR:
		return -EISDIR;
	default:
		return -EINVAL;
	}
}

/**
 * @brief change permission of a file/dir
 * @param node inode of the file/dir
 * @param perm new permission
 * @return 0 on succes else error code
 */
static inline int vfs_chmod(vfs_node_t *node, mode_t perm) {
	struct stat st;
	st.st_mode = perm;
	return vfs_setattr(node, &st, VNODE_ATTR_MODE);
}

/**
 * @brief change owner of a file/dir
 * @param node inode for the file/dir
 * @param owner uid of new owner
 * @param group_owner gid of new group_owner
 * @return 0 on succes else error code
 */
static inline int vfs_chown(vfs_node_t *node, uid_t owner, gid_t group_owner) {
	struct stat st;
	st.st_uid = owner;
	st.st_gid = group_owner;
	vfs_node_lock(node);

	// clear setuid bit and setgid bit
	st.st_mode = node->mode & ~(S_ISUID | S_ISGID);

	int ret = vfs_raw_setattr(node, &st, VNODE_ATTR_UID | VNODE_ATTR_GID);
	vfs_node_unlock(node);
	return ret;
}

/**
 * @brief change times of a file/dir
 * @param node inode for the file/dir
 * @param times the times (0 is atime and 1 is mtime)
 * @return 0 on succes else error code
 */
static inline int vfs_utimes(vfs_node_t *node, const struct timeval times[2]) {
	struct stat st;
	st.st_atime = times[0].tv_sec;
	st.st_mtime = times[1].tv_sec;
	return vfs_setattr(node, &st, VNODE_ATTR_ATIME | VNODE_ATTR_MTIME);
}

static inline int vfs_update_time(vfs_node_t *node, int mask) {
	struct stat st;
	st.st_atime = st.st_mtime = st.st_ctime = gettime_sec(CLOCK_REALTIME);
	return vfs_setattr(node, &st, mask);
}

vfs_dentry_t *vfs_lookup(vfs_dentry_t *entry, const char *name);
ssize_t vfs_readlink(vfs_node_t *node, char *buf, size_t bufsiz);
/**
 * @brief write an inode back to disk
 * @param node the node to writeback
 */
static inline int vfs_node_write(vfs_node_t *node) {
	if (!node) return -EBADF;
	if (!atomic_fetch_and(&node->flags, ~VNODE_FLAG_DIRTY)) {
		// not dirty
		return 0;
	}
	kassert(node->superblock);
	if (!node->superblock->ops || !node->superblock->ops->write_inode) return -EOPNOTSUPP;
	return node->superblock->ops->write_inode(node->superblock, node);
}

// fds operations
/**
 * @brief open a context for a given path relative to at
 * @param at
 * @param path the path (even if this absolute it will be interptreted as relative)
 * @param flags open flags (VFS_READONLY,...)
 * @return a pointer to the vfs_node_t on success, else an error ptr (check with IS_ERR)
 */
vfs_fd_t *vfs_open_at(vfs_dentry_t *at, const char *path, long flags, ...);

/**
 * @brief open a context for a given path (absolute)
 * @param path
 * @param flags open flags (VFS_READONLY,...)
 * @return a pointer to the vfs_node_t on success, else an error ptr (check with IS_ERR)
 */
static inline vfs_fd_t *vfs_open(const char *path, long flags, ...) {
	mode_t mode = 0777;
	if (flags & O_CREAT) {
		va_list args;
		va_start(args, flags);
		mode = va_arg(args, mode_t);
		va_end(args);
	}
	return vfs_open_at(NULL, path, flags, mode);
}

/**
 * @brief open an inode
 * @param node the inode to open
 * @param dentry the dentry of the inodes
 * @param flags the flags to open with
 * @return a vfs fd on success, else an error ptr (check with IS_ERR)
 */
vfs_fd_t *vfs_open_node(vfs_node_t *node, vfs_dentry_t *dentry, long flags);

/**
 * @brief close a file descritor
 * @param fd the fd to close
 */
void vfs_close(vfs_fd_t *fd);

ssize_t vfs_read(vfs_fd_t *node, void *buffer, uint64_t offset, size_t count);
ssize_t vfs_write(vfs_fd_t *node, const void *buffer, uint64_t offset, size_t count);


/**
 * @brief generic seek operations for file/devices
 */
static inline off_t vfs_generic_seek(vfs_fd_t *fd, off_t offset, int whence) {
	struct stat st;
	vfs_getattr(fd->inode, &st);

	switch (whence) {
	case SEEK_SET:
		fd->offset = offset;
		break;
	case SEEK_CUR:
		fd->offset += offset;
		break;
	case SEEK_END:
		fd->offset = st.st_size + offset;
		break;
	default:
		return -EINVAL;
	}

	return fd->offset;
}

static inline off_t vfs_seek(vfs_fd_t *fd, off_t offset, int whence) {
	switch (fd->type) {
	case S_IFREG:
	case S_IFBLK:
		if (fd->ops && fd->ops->seek) {
			return fd->ops->seek(fd, offset, whence);
		} else {
			return vfs_generic_seek(fd, offset, whence);
		}
	case S_IFCHR:
		if (fd->ops && fd->ops->seek) {
			return fd->ops->seek(fd, offset, whence);
		} else {
			return -ESPIPE;
		}
	case S_IFDIR:
		return -EISDIR;
	case S_IFLNK:
		return -EBADF;
	case S_IFSOCK:
	case S_IFIFO:
		return -ESPIPE;
	}
}

static inline ssize_t vfs_user_read(vfs_fd_t *fd, void *buffer, size_t size) {
	ssize_t ret = vfs_read(fd, buffer, fd->offset, size);
	if (ret > 0) {
		fd->offset += ret;
	}
	return ret;
}

static inline ssize_t vfs_user_write(vfs_fd_t *fd, const void *buffer, size_t size) {
	if (fd->flags & O_APPEND) {
		// seek to the end before each write
		vfs_seek(fd, 0, SEEK_END);
	}
	ssize_t ret = vfs_write(fd, buffer, fd->offset, size);
	if (ret > 0) {
		fd->offset += ret;
	}
	return ret;
}

static inline int vfs_mmap(vfs_fd_t *fd, off_t offset, struct vmm_seg *seg) {
	if (!fd) return -EBADF;
	if (!fd->ops->mmap) return -ENODEV;
	return fd->ops->mmap(fd, offset, seg);
}

int vfs_poll_add(vfs_fd_t *fd, struct poll_event *event);
int vfs_poll_remove(vfs_fd_t *fd, struct poll_event *event);
int vfs_poll_get(vfs_fd_t *fd, struct poll_event *event);

int vfs_readdir(vfs_node_t *node, unsigned long index, struct dirent *dirent);

/**
 * @brief duplicate a file descriptor
 * @param fd the fd to duplicate
 * @return the new fd
 */
static inline vfs_fd_t *vfs_dup(vfs_fd_t *fd) {
	if (fd) ref_count_inc(&fd->ref_count);
	return fd;
}

vfs_fd_t *vfs_fd_alloc(void);

/**
 * @brief get a path from a dentry
 * @param dentry the dentry to get the path of
 * @return a dynamicly allocated string that must be freed !
 */
char *vfs_dentry_path(vfs_dentry_t *dentry);

/**
 * @brief device specific operation
 * @param node the context of the file/dev
 * @param request the id of the request
 * @param arg device/request specific
 * @return device/request specific
 */
int vfs_ioctl(vfs_fd_t *fd, long request, void *arg);

void vfs_register_fs(vfs_filesystem_t *fs);
void vfs_unregister_fs(vfs_filesystem_t *fs);
int vfs_auto_mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data);

int vfs_perm(vfs_node_t *node);
int vfs_user_perm(vfs_node_t *node, uid_t uid, gid_t gid);

#define PERM_READ    04
#define PERM_WRITE   02
#define PERM_EXECUTE 01

static vfs_dentry_t *vfs_dentry_ref(vfs_dentry_t *dentry) {
	if (dentry) ref_count_inc(&dentry->ref_count);
	return dentry;
}

vfs_dentry_t *vfs_get_dentry_at(vfs_dentry_t *at, const char *path, long flags, ...);

static inline vfs_dentry_t *vfs_get_dentry(const char *path, long flags, ...) {
	mode_t mode = 0777;
	if (flags & O_CREAT) {
		va_list args;
		va_start(args, flags);
		mode = va_arg(args, mode_t);
		va_end(args);
	}
	return vfs_get_dentry_at(NULL, path, flags, mode);
}

void vfs_dentry_release(vfs_dentry_t *dentry);

vfs_dentry_t *vfs_dentry_allocate(void);
vfs_dentry_t *vfs_get_root(void);
void vfs_set_root(vfs_dentry_t *dentry);
void dentry_add_lru(vfs_dentry_t *dentry);
void vfs_dentry_remove_lru(vfs_dentry_t *dentry);
void vfs_dentry_add(vfs_dentry_t *parent, vfs_dentry_t *child);
void vfs_dentry_remove(vfs_dentry_t *dentry);

static inline vfs_node_t *vfs_node_cache_lookup(vfs_superblock_t *superblock, vfs_dentry_t *dentry) {
	vfs_node_t *node = dentry->inode;
	if (node) {
		return vfs_node_ref(node);
	}
	rcu_acquire_read(&superblock->inodes.rcu);
	node = xarray_get(&superblock->inodes, dentry->inode_number);
	if (node) {
		dentry->inode = vfs_node_ref(node);
		rcu_release_read(&superblock->inodes.rcu);
		return vfs_node_ref(node);
	}
	rcu_release_read(&superblock->inodes.rcu);
	return NULL;
}

static inline int vfs_dentry_is_negative(vfs_dentry_t *dentry) {
	return dentry->inode == NULL;
}

// flags
#define O_PARENT 0x4000 // open the parent

#endif
