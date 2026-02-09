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

#define VFS_FILE  0x001
#define VFS_DIR   0x002
#define VFS_LINK  0x004
#define VFS_MOUNT 0x010
#define VFS_CHAR  0x020
#define VFS_BLOCK 0x040
#define VFS_TTY   0x080
#define VFS_SOCK  0x100

struct vfs_node;
struct vmm_seg;
struct vfs_ops;
struct superblock;

typedef struct vfs_node {
	void *private_inode;
	void *private_inode2;
	struct vfs_superblock *superblock;
	struct vfs_ops *ops;
	struct vfs_node *linked_node; // used for mount point
	long flags;
	size_t ref_count;
	spinlock_t lock;
} vfs_node_t;

/**
 * @brief represent a directory entry
 */
typedef struct vfs_dentry {
	list_node_t node;
	char name[256];
	vfs_node_t *inode;
	ino_t inode_number;
	int type;
	int flags;
	struct vfs_dentry *parent;
	struct vfs_dentry *old; // used for mount point
	list_t children;
	size_t ref_count;
} vfs_dentry_t;

#define VFS_DENTRY_MOUNT 0x01 // the dentry is a mount point

/**
 * @brief represent an open context with a file/dir/device
 */
typedef struct vfs_fd {
	vfs_node_t *inode;
	struct vfs_ops *ops;
	void *private;
	size_t ref_count;
	long flags;
	long type;
} vfs_fd_t;

/**
 * @brief all operations that can be done on a \ref vfs_fd_t or a \ref vfs_node_t
 */
typedef struct vfs_ops {
	// inode operations
	int (*lookup)(vfs_node_t *, vfs_dentry_t *, const char *name);
	int (*create)(vfs_node_t *, const char *name, mode_t perm, long, void *);
	int (*unlink)(vfs_node_t *, const char *);
	int (*link)(vfs_node_t *, const char *, vfs_node_t *, const char *);
	int (*symlink)(vfs_node_t *, const char *, const char *);
	ssize_t(*readlink)(vfs_node_t *, char *, size_t);
	int (*setattr)(vfs_node_t *, struct stat *);
	int (*getattr)(vfs_node_t *, struct stat *);
	int (*truncate)(vfs_node_t *, size_t);
	void (*cleanup)(vfs_node_t *);

	// fd operations
	int (*open)(vfs_fd_t *);
	int (*readdir)(vfs_fd_t *, unsigned long index, struct dirent *);
	ssize_t(*read)(vfs_fd_t *, void *buf, off_t off, size_t count);
	ssize_t(*write)(vfs_fd_t *, const void *buf, off_t off, size_t count);
	int (*ioctl)(vfs_fd_t *, long, void *);
	int (*wait_check)(vfs_fd_t *, short);
	int (*wait)(vfs_fd_t *, short);
	int (*mmap)(vfs_fd_t *, off_t, struct vmm_seg *);
	void (*close)(vfs_fd_t *);
} vfs_ops_t;

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

typedef struct vfs_filesystem_struct {
	list_node_t node;
	char name[16];
	int (*mount)(const char *source, const char *target, unsigned long flags, const void *data, vfs_superblock_t **mount_point);
} vfs_filesystem_t;

void init_vfs(void);

// inode operations
//
/**
 * @brief mount a vfs_node_t to the specified path and create a directory fpr it if needed
 * @param path the path to mount to
 * @param local_root the vfs_node_t to mount
 * @return 0 on success else error code
 */
int vfs_mount(const char *path, vfs_superblock_t *superblock);


int vfs_mount_on(vfs_dentry_t *mount_point, vfs_superblock_t *superblock);
int vfs_mountat(vfs_dentry_t *at, const char *name, vfs_superblock_t *superblock);

int vfs_chroot(vfs_node_t *new_root);


vfs_dentry_t *vfs_lookup(vfs_dentry_t *entry, const char *name);
ssize_t vfs_read(vfs_fd_t *node, void *buffer, uint64_t offset, size_t count);
ssize_t vfs_write(vfs_fd_t *node, const void *buffer, uint64_t offset, size_t count);
int vfs_create(const char *path, int perm, long flags);
int vfs_createat(vfs_dentry_t *at, const char *path, int perm, long flags);
int vfs_create_ext(const char *path, int perm, long flags, void *arg);
int vfs_createat_ext(vfs_dentry_t *at, const char *path, int perm, long flags, void *arg);

static inline int vfs_mkdir(const char *path, mode_t perm) {
	return vfs_create(path, perm, VFS_DIR);
}

/**
 * @brief unlink a path
 * @param path the path to unlink
 * @return 0 on success else error code
 */
int vfs_unlink(const char *path);

int vfs_link(const char *src, const char *dest);

int vfs_symlink(const char *target, const char *linkpath);
ssize_t vfs_readlink(vfs_node_t *node, char *buf, size_t bufsiz);
int vfs_getattr(vfs_node_t *node, struct stat *st);
int vfs_setattr(vfs_node_t *node, struct stat *st);

int vfs_unmount(const char *path);
int vfs_unmountat(vfs_dentry_t *at, const char *path);

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
vfs_fd_t *vfs_openat(vfs_dentry_t *at, const char *path, long flags);

/**
 * @brief open a context for a given path (absolute)
 * @param path
 * @param flags open flags (VFS_READONLY,...)
 * @return an pointer to the vfs_node_t or NULL if fail
 */
static inline vfs_fd_t *vfs_open(const char *path, long flags) {
	return vfs_openat(NULL, path, flags);
}

/**
 * @brief open an inode
 * @param node the inode to open
 * @param flags the flags to open with
 */
vfs_fd_t *vfs_open_node(vfs_node_t *node, long flags);

/**
 * @brief close a file descritor
 * @param fd the fd to close
 */
void vfs_close(vfs_fd_t *fd);


int vfs_readdir(vfs_fd_t *fd, unsigned long index, struct dirent *dirent);

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

static vfs_dentry_t *vfs_dup_dentry(vfs_dentry_t *dentry) {
	if (dentry) dentry->ref_count++;
	return dentry;
}

vfs_dentry_t *vfs_get_dentry_at(vfs_dentry_t *at, const char *path, long flags);
vfs_dentry_t *vfs_get_dentry(const char *path, long flags);
void vfs_release_dentry(vfs_dentry_t *dentry);

static void vfs_add_dentry(vfs_dentry_t *parent, vfs_dentry_t *child) {
	// child hold a ref to the parent
	vfs_dup_dentry(parent);
	list_append(&parent->children, &child->node);
}

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
	return dentry->inode != NULL;
}

//flags
#define O_PARENT       0x4000 //open the parent

#endif
