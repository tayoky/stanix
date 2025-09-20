#ifndef VFS_H
#define VFS_H

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stddef.h>
#include <dirent.h>
#include <limits.h>


//just in case we use a weird limits.h
#ifndef PATH_MAX
#define PATH_MAX 256
#endif

#define VFS_FILE  0x01
#define VFS_DIR   0x02
#define VFS_LINK  0x04
#define VFS_DEV   0x08
#define VFS_MOUNT 0x10
#define VFS_CHAR  0x20
#define VFS_BLOCK 0x40
#define VFS_TTY   0x80

struct vfs_node_struct;
struct vfs_mount_point_struct;
struct memseg_struct;

typedef struct vfs_node_struct {
	void *private_inode;
	struct vfs_mount_point_struct *mount_point;
	uint64_t flags;
	uint64_t ref_count;
	ssize_t (* read)(struct vfs_node_struct *,void *buf,uint64_t off,size_t count);
	ssize_t (* write)(struct vfs_node_struct *,void *buf,uint64_t off,size_t count);
	void (* close)(struct vfs_node_struct *);
	struct vfs_node_struct *(* lookup)(struct vfs_node_struct *,const char *name);
	int (* create)(struct vfs_node_struct*,const char *name,mode_t perm,long);
	int (* unlink)(struct vfs_node_struct*,const char *);
	int (* link)(struct vfs_node_struct*,const char*,struct vfs_node_struct*,const char*);
	int (* symlink)(struct vfs_node_struct*,const char*,const char*);
	ssize_t (*readlink)(struct vfs_node_struct*,char*,size_t);
	struct dirent *(* readdir)(struct vfs_node_struct*,uint64_t index);
	int (* truncate)(struct vfs_node_struct*,size_t);
	int (* ioctl)(struct vfs_node_struct*,uint64_t,void*);
	int (* getattr)(struct vfs_node_struct *,struct stat *);
	int (* setattr)(struct vfs_node_struct *,struct stat *);
	int (* wait_check)(struct vfs_node_struct *,short);
	int (* wait)(struct vfs_node_struct *,short);
	int (* mmap)(struct vfs_node_struct *,off_t,struct memseg_struct *);

	//used for directories cache
	char name[PATH_MAX];
	struct vfs_node_struct *parent;
	struct vfs_node_struct *brother;
	struct vfs_node_struct *child;
	size_t childreen_count;

	struct vfs_node_struct *linked_node; //used from mount point
}vfs_node;

typedef struct vfs_mount_point_struct{
	char name[PATH_MAX];
	struct vfs_mount_point_struct *prev;
	struct vfs_mount_point_struct *next;
	vfs_node *root;
	uint64_t flags;
}vfs_mount_point;

typedef struct vfs_filesystem_struct {
	char name[16];
	int (*mount)(const char *source,const char *target,unsigned long flags,const void *data);
} vfs_filesystem;

struct kernel_table_struct;
void init_vfs(void);

/// @brief mount a vfs_node to the specified path and create a directory fpr it if needed
/// @param path the path to mount to
/// @param local_root the vfs_node to mount
/// @return 0 on success else error code
int vfs_mount(const char *path,vfs_node *local_root);

int vfs_chroot(vfs_node *new_root);

/// @brief open a context for a given path (absolute)
/// @param path 
/// @param flags open flags (VFS_READONLY,...)
/// @return an pointer to the vfs_node or NULL if fail
vfs_node *vfs_open(const char *path,uint64_t flags);

/// @brief open a context for a given path relative to at
/// @param at 
/// @param path the path (even if this absolute it will be interptreted as relative)
/// @param flags open flags (VFS_READONLY,...)
/// @return an pointer to the vfs_node context or NULL if an error happend
vfs_node *vfs_openat(vfs_node *at,const char *path,uint64_t flags);

vfs_node *vfs_lookup(vfs_node *node,const char *name);
ssize_t vfs_read(vfs_node *node,void *buffer,uint64_t offset,size_t count);
ssize_t vfs_write(vfs_node *node,const void *buffer,uint64_t offset,size_t count);
int vfs_create(const char *path,int perm,uint64_t flags);
int vfs_mkdir(const char *path,int perm);

/// @brief close an context
/// @param node the context to close
void vfs_close(vfs_node *node);

/// @brief unlink a path
/// @param path the path to unlink
/// @return 0 on success else error code
int vfs_unlink(const char *path);


int vfs_link(const char *src,const char *dest);

int vfs_symlink(const char *target, const char *linkpath);
ssize_t vfs_readlink(vfs_node *node,char *buf,size_t bufsiz);


/// @brief read an entry in a directory at a specifed index
/// @param node context of the dir
/// @param index the index to read
/// @return a pointer to a dirent that can be free or NULL if fail
struct dirent *vfs_readdir(vfs_node *node,uint64_t index);

/// @brief truncate a file to a specfied size
/// @param node context of the file
/// @param size the new size
/// @return 0 on success else error code
int vfs_truncate(vfs_node *node,size_t size);

/// @brief change permission of a file/dir
/// @param node context of the file/dir
/// @param perm new permission
/// @return 0 on succes else error code
int vfs_chmod(vfs_node *node,mode_t perm);

/// @brief change owner of a file/dir
/// @param node context for the file/dir
/// @param owner uid of new owner
/// @param group_owner gid of new group_owner
/// @return 0 on succes else error code
int vfs_chown(vfs_node *node,uid_t owner,gid_t group_owner);

/// @brief device specific operation
/// @param node the context of the file/dev
/// @param request the id of the request
/// @param arg device/request specific
/// @return device/request specific
int vfs_ioctl(vfs_node *node,uint64_t request,void *arg);

/// @brief duplicate an vfs_node
/// @param node the vfs_node to duplicate
/// @return the new vfs_node
vfs_node *vfs_dup(vfs_node *node);

/// @brief check if a vfs_node is ready for write/read
/// @param node the node to check
/// @param type the type to check (read and/or write)
/// @return 1 if is ready or 0 if not
int vfs_wait_check(vfs_node *node,short type);

/// @brief wait on a file for read/write
/// @param node the node to wait for
/// @param type the type of action (write and/or read)
/// @return 0 or -INVAL
/// @note vfs_wait don't block the wait start only after calling block_brock()
int vfs_wait(vfs_node *node,short type);


int vfs_getattr(vfs_node *node,struct stat *st);
int vfs_setattr(vfs_node *node,struct stat *st);

int vfs_unmount(const char *path);

int vfs_mmap(vfs_node *node,off_t offset,struct memseg_struct *seg);

void vfs_register_fs(vfs_filesystem *fs);
void vfs_unregister_fs(vfs_filesystem *fs);
int vfs_auto_mount(const char *source,const char *target,const char *filesystemtype,unsigned long mountflags,const void *data);

//flags
#define VFS_READONLY     0x01 //readonly
#define VFS_WRITEONLY    0x02 //write only
#define VFS_READWRITE    0x03 //write and read
#define VFS_PARENT       0x04 //open the parent
#define VFS_NOFOLOW      0x08 //don't folow symlinks

#endif