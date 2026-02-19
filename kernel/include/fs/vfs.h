#ifndef _KERNEL_VFS_H
#define _KERNEL_VFS_H

#include <kernel/spinlock.h>
#include <kernel/hashmap.h>
#include <kernel/list.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stddef.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>

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

#define VFS_FILE  0x001
#define VFS_DIR   0x002
#define VFS_LINK  0x004
#define VFS_CHAR  0x020
#define VFS_BLOCK 0x040
#define VFS_FIFO  0x080
#define VFS_SOCK  0x100

struct vfs_node;
struct vmm_seg;
struct vfs_inode_ops;
struct vfs_fd_ops;
struct superblock;

typedef struct vfs_node {
	void *private_inode;
	struct vfs_superblock *superblock;
	struct vfs_inode_ops *ops;
	struct vfs_node *linked_node; // used for mount point
	long flags;
	size_t ref_count;
	spinlock_t lock;
} vfs_node_t;

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
	size_t ref_count;
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
	size_t ref_count;
	long flags;
	long type;
	off_t offset;
} vfs_fd_t;

/**
 * @brief all operations that can be done on a \ref vfs_node_t
 */
typedef struct vfs_inode_ops {
	int (*lookup)(vfs_node_t *, vfs_dentry_t *, const char *name);
	int (*readdir)(vfs_node_t *, unsigned long index, struct dirent *);
	int (*create)(vfs_node_t *, vfs_dentry_t *, mode_t perm);
	int (*mkdir)(vfs_node_t *, vfs_dentry_t *, mode_t perm);
	int (*mknod)(vfs_node_t *, vfs_dentry_t *, mode_t perm, dev_t dev);
	int (*unlink)(vfs_node_t *, vfs_dentry_t *);
	int (*rmdir)(vfs_node_t *, vfs_dentry_t *);
	int (*rename)(vfs_node_t *old_dir, vfs_dentry_t *old_dentry, vfs_node_t *new_dir, vfs_dentry_t *new_dentry, int flags);
	int (*link)(vfs_dentry_t *old_dentry, vfs_node_t *new_dir, vfs_dentry_t *new_dentry);
	int (*symlink)(vfs_node_t *, vfs_dentry_t *, const char *target);
	ssize_t(*readlink)(vfs_node_t *, char *, size_t);
	int (*setattr)(vfs_node_t *, struct stat *);
	int (*getattr)(vfs_node_t *, struct stat *);
	int (*truncate)(vfs_node_t *, size_t);
	void (*cleanup)(vfs_node_t *);
	int (*open)(vfs_fd_t *);
} vfs_inode_ops_t;

/**
 * @brief contain all \ref vfs_fd_t operations
 */
typedef struct vfs_fd_ops {
	int (*open)(vfs_fd_t *);
	ssize_t(*read)(vfs_fd_t *, void *buf, off_t off, size_t count);
	ssize_t(*write)(vfs_fd_t *, const void *buf, off_t off, size_t count);
	int (*ioctl)(vfs_fd_t *, long, void *);
	int (*wait_check)(vfs_fd_t *, short);
	int (*wait)(vfs_fd_t *, short);
	int (*mmap)(vfs_fd_t *, off_t, struct vmm_seg *);
	void (*close)(vfs_fd_t *);
	off_t (*seek)(vfs_fd_t *, off_t offset, int whence);
} vfs_fd_ops_t;

typedef struct vfs_superblock_ops {
	void (*destroy)(struct vfs_superblock *superblock);
} vfs_superblock_ops_t;

typedef struct vfs_superblock {
	list_node_t node;
	hashmap_t inodes;
	vfs_node_t *root;
	vfs_superblock_ops_t *ops;
	vfs_fd_t *device;
	long flags;
	char name[PATH_MAX];
} vfs_superblock_t;

#define VFS_SUPERBLOCK_NO_DCACHE 0x01

typedef struct vfs_filesystem_struct {
	list_node_t node;
	char name[16];
	int (*mount)(const char *source, const char *target, unsigned long flags, const void *data, vfs_superblock_t **mount_point);
} vfs_filesystem_t;

void init_vfs(void);

// inode operations

int vfs_mount_on(vfs_dentry_t *mount_point, vfs_superblock_t *superblock);

int vfs_chroot(vfs_dentry_t *new_root);


vfs_dentry_t *vfs_lookup(vfs_dentry_t *entry, const char *name, int *status);
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
	if (fd->type == VFS_DIR) {
		return -EISDIR;
	} else if (fd->ops && fd->ops->seek) {
		return fd->ops->seek(fd, offset, whence);
	} else if(fd->type == VFS_FILE || fd->type == VFS_BLOCK || fd->type == VFS_CHAR) {
		return vfs_generic_seek(fd, offset, whence);
	} else {
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

int vfs_create_at(vfs_dentry_t *at, const char *path, mode_t mode);
int vfs_mkdir_at(vfs_dentry_t *at, const char *path, mode_t mode);
int vfs_mknod_at(vfs_dentry_t *at, const char *path, mode_t mode, dev_t dev);
int vfs_link_at(vfs_dentry_t *old_at, const char *old_path, vfs_dentry_t *new_at, const char *new_path);
int vfs_symlink_at(const char *target, vfs_dentry_t *at, const char *path);
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

static inline vfs_unlink(const char *path) {
	return vfs_unlink_at(NULL, path);
}

static inline vfs_rmdir(const char *path) {
	return vfs_rmdir_at(NULL, path);
}

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

ssize_t vfs_readlink(vfs_node_t *node, char *buf, size_t bufsiz);
int vfs_getattr(vfs_node_t *node, struct stat *st);
int vfs_setattr(vfs_node_t *node, struct stat *st);

vfs_node_t *vfs_get_node_at(vfs_dentry_t *at, const char *pathname, long flags);
static inline vfs_node_t *vfs_get_node(const char *pathname, long flags) {
	return vfs_get_node_at(NULL, pathname, flags);
}

static inline vfs_node_t *vfs_dup_node(vfs_node_t *node) {
	if (node) node->ref_count++;
	return node;
}

void vfs_close_node(vfs_node_t *node);


// fds operations
/**
 * @brief open a context for a given path relative to at
 * @param at
 * @param path the path (even if this absolute it will be interptreted as relative)
 * @param flags open flags (VFS_READONLY,...)
 * @return an pointer to the vfs_node_t context or NULL if an error happend
 */
vfs_fd_t *vfs_open_at(vfs_dentry_t *at, const char *path, long flags);

/**
 * @brief open a context for a given path (absolute)
 * @param path
 * @param flags open flags (VFS_READONLY,...)
 * @return an pointer to the vfs_node_t or NULL if fail
 */
static inline vfs_fd_t *vfs_open(const char *path, long flags) {
	return vfs_open_at(NULL, path, flags);
}

/**
 * @brief open an inode
 * @param node the inode to open
 * @param dentry the dentry of the inodes
 * @param flags the flags to open with
 */
vfs_fd_t *vfs_open_node(vfs_node_t *node, vfs_dentry_t *dentry, long flags);

/**
 * @brief close a file descritor
 * @param fd the fd to close
 */
void vfs_close(vfs_fd_t *fd);


int vfs_readdir(vfs_node_t *node, unsigned long index, struct dirent *dirent);

/**
 * @brief duplicate a file descriptor
 * @param fd the fd to duplicate
 * @return the new fd
 */
static inline vfs_fd_t *vfs_dup(vfs_fd_t *fd) {
	if (fd) fd->ref_count++;
	return fd;
}

/**
 * @brief get a path from a dentry
 * @param dentry the dentry to get the path of
 * @return a dynamicly allocated string that must be freed !
 */
char *vfs_dentry_path(vfs_dentry_t *dentry);

/**
 * @brief truncate a file to a specfied size
 * @param node context of the file
 * @param size the new size
 * @return 0 on success else error code
 */
static inline int vfs_truncate(vfs_node_t *node, size_t size) {
	if (!node || !node->ops->truncate) return -EBADF;
	if (node->flags & VFS_DIR) {
		return -EISDIR;
	}
	return node->ops->truncate(node, size);
}

/**
 * @brief change permission of a file/dir
 * @param node context of the file/dir
 * @param perm new permission
 * @return 0 on succes else error code
 */
int vfs_chmod(vfs_node_t *node, mode_t perm);

/**
 * @brief change owner of a file/dir
 * @param node context for the file/dir
 * @param owner uid of new owner
 * @param group_owner gid of new group_owner
 * @return 0 on succes else error code
 */
int vfs_chown(vfs_node_t *node, uid_t owner, gid_t group_owner);

/**
 * @brief device specific operation
 * @param node the context of the file/dev
 * @param request the id of the request
 * @param arg device/request specific
 * @return device/request specific
 */
int vfs_ioctl(vfs_fd_t *fd, long request, void *arg);

/**
 * @brief check if a vfs_node_t is ready for write/read
 * @param node the node to check
 * @param type the type to check (read and/or write)
 * @return 1 if is ready or 0 if not
 */
int vfs_wait_check(vfs_fd_t *node, short type);

/**
 * @brief wait on a file for read/write
 * @param node the node to wait for
 * @param type the type of action (write and/or read)
 * @return 0 or -INVAL
 * @note vfs_wait don't block the wait start only after calling block_task()
 * @note this is unimplemented
 */
int vfs_wait(vfs_fd_t *node, short type);

static inline int vfs_mmap(vfs_fd_t *fd, off_t offset, struct vmm_seg *seg) {
	if (!fd || !fd->ops->mmap) return -EBADF;
	return fd->ops->mmap(fd, offset, seg);
}

void vfs_register_fs(vfs_filesystem_t *fs);
void vfs_unregister_fs(vfs_filesystem_t *fs);
int vfs_auto_mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data);

int vfs_perm(vfs_node_t *node);
int vfs_user_perm(vfs_node_t *node, uid_t uid, gid_t gid);

#define PERM_READ    04
#define PERM_WRITE   02
#define PERM_EXECUTE 01

static vfs_dentry_t *vfs_dup_dentry(vfs_dentry_t *dentry) {
	if (dentry) dentry->ref_count++;
	return dentry;
}

vfs_dentry_t *vfs_get_dentry_at(vfs_dentry_t *at, const char *path, long flags);

static inline vfs_dentry_t *vfs_get_dentry(const char *path, long flags) {
	return vfs_get_dentry_at(NULL, path, flags);
}

void vfs_release_dentry(vfs_dentry_t *dentry);

static vfs_node_t *vfs_node_cache_lookup(vfs_superblock_t *superblock, vfs_dentry_t *dentry) {
	vfs_node_t *node = dentry->inode;
	if (node) {
		return vfs_dup_node(node);
	}
	node = hashmap_get(&superblock->inodes, dentry->inode_number);
	if (node) {
		dentry->inode = vfs_dup_node(node);
		return vfs_dup_node(node);
	}
	return NULL;
}

static int vfs_dentry_is_negative(vfs_dentry_t *dentry) {
	return dentry->inode == NULL;
}

//flags
#define O_PARENT       0x4000 //open the parent

#endif
