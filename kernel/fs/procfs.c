#include <kernel/scheduler.h>
#include <kernel/string.h>
#include <kernel/kheap.h>
#include <kernel/print.h>
#include <kernel/procfs.h>
#include <kernel/vfs.h>
#include <errno.h>

typedef struct proc_inode {
	process_t *proc;
	int type;
} proc_inode_t;

#define INODE_SELF 1
#define INODE_DIR 2
#define INODE_CWD 3

static vfs_ops_t proc_ops;
long strtol(const char *str, char **end, int base);

static vfs_node_t *proc_new_node(process_t *proc, int type) {
	proc_inode_t *inode = kmalloc(sizeof(proc_inode_t));
	inode->proc = proc;
	inode->type = type;

	vfs_node_t *node = kmalloc(sizeof(vfs_node_t));
	memset(node, 0, sizeof(vfs_node_t));
	node->private_inode = inode;
	node->ops           = &proc_ops;
	switch (type) {
	case INODE_SELF:
	case INODE_CWD:
		node->flags = VFS_LINK;
		break;
	case INODE_DIR:
		node->flags = VFS_DIR;
		break;
	}
	return node;
} 

static int proc_readdir(vfs_fd_t *fd, unsigned long index, struct dirent *dirent) {
	(void)fd;
	static char *content[] = {
		".",
		"..",
		"cwd",
	};

	if (index >= sizeof(content) / sizeof(*content))return -ENOENT;

	strcpy(dirent->d_name, content[index]);
	return 0;
}

static int proc_getattr(vfs_node_t *node, struct stat *st) {
	proc_inode_t *inode = node->private_inode;
	st->st_uid  = inode->proc->euid;
	st->st_gid  = inode->proc->egid;
	st->st_mode = 0555;
	st->st_ino  = (inode->proc->pid << 3) | inode->type;
	return 0;
}

static ssize_t proc_readlink(vfs_node_t *node, char *buffer, size_t count) {
	proc_inode_t *inode = node->private_inode;
	static char buf[64];
	switch (inode->type) {
	case INODE_CWD:
		if (count > strlen(inode->proc->cwd_path) + 1) count = strlen(inode->proc->cwd_path) + 1;
		memcpy(buffer, inode->proc->cwd_path, count);
		break;
	case INODE_SELF:
		sprintf(buf, "/proc/%ld", inode->proc->pid);
		if (count > strlen(buf) + 1) count = strlen(buf) + 1;
		memcpy(buffer, buf, count);
		break;
	}
	return count;
}

static vfs_node_t *proc_lookup(vfs_node_t *node, const char *name) {
	proc_inode_t *inode = node->private_inode;

	if (!strcmp(name, "cwd")) {
		return proc_new_node(inode->proc, INODE_CWD);
	}
	return NULL;
}

static void proc_cleanup(vfs_node_t *node) {
	proc_inode_t *inode = node->private_inode;

	kfree(inode);
}

static vfs_ops_t proc_ops = {
	.lookup   = proc_lookup,
	.readdir  = proc_readdir,
	.readlink = proc_readlink,
	.getattr  = proc_getattr,
	.cleanup  = proc_cleanup,
};

static vfs_node_t *proc_root_lookup(vfs_node_t *root, const char *name) {
	(void)root;
	if (!strcmp(name, "self")) {
		return proc_new_node(get_current_proc(), INODE_SELF);
	}
	char *end;
	pid_t pid = strtol(name, &end, 10);
	if (end == name)return NULL;
	process_t *proc = pid2proc(pid);
	if (!proc)return NULL;


	return proc_new_node(proc, INODE_DIR);
}

static int proc_root_readdir(vfs_fd_t *fd, unsigned long index, struct dirent *dirent) {
	(void)fd;
	if (index == 0) {
		strcpy(dirent->d_name, ".");
		return 0;
	}

	if (index == 1) {
		strcpy(dirent->d_name, "..");
		return 0;
	}

	if (index == 2) {
		strcpy(dirent->d_name, "self");
		return 0;
	}

	index -=3;
	foreach(node, &proc_list) {
		if (!index) {
			process_t *proc = container_from_node(process_t*, proc_list_node, node);
			sprintf(dirent->d_name, "%d", proc->pid);
			return 0;
		}
		index--;
	}
	return -ENOENT;
}

static vfs_ops_t proc_root_ops = {
	.readdir = proc_root_readdir,
	.lookup  = proc_root_lookup,
};

int proc_mount(const char *source, const char *target, unsigned long flags, const void *data) {
	(void)data;
	(void)source;
	(void)flags;

	vfs_node_t *node = kmalloc(sizeof(vfs_node_t));
	memset(node, 0, sizeof(vfs_node_t));
	node->flags   = VFS_DIR;
	node->ops     = &proc_root_ops;

	return vfs_mount(target, node);
}

vfs_filesystem_t proc_fs = {
	.name = "proc",
	.mount = proc_mount,
};

void init_proc(void) {
	kdebugf("init proc fs ...");
	vfs_register_fs(&proc_fs);
	kok();
}