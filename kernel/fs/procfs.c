#include <kernel/kheap.h>
#include <kernel/print.h>
#include <kernel/process.h>
#include <kernel/procfs.h>
#include <kernel/string.h>
#include <kernel/vfs.h>
#include <kernel/vmm.h>
#include <kernel/xarray.h>
#include <errno.h>

typedef struct proc_inode {
	vfs_node_t vnode;
	process_t *proc;
	int type;
} proc_inode_t;

#define INODE_SELF    1
#define INODE_DIR     2
#define INODE_CWD     3
#define INODE_EXE     4
#define INODE_MAPS    5
#define INODE_CMDLINE 6
#define INODE_FD_DIR  7
#define INODE_STATUS  8

static vfs_inode_ops_t proc_inode_ops;
static vfs_fd_ops_t proc_fd_ops;
long strtol(const char *str, char **end, int base);

static vfs_node_t *proc_new_node(vfs_superblock_t *superblock, process_t *proc, int type) {
	proc_inode_t *inode = kmalloc(sizeof(proc_inode_t));
	memset(inode, 0, sizeof(proc_inode_t));
	inode->proc = proc;
	inode->type = type;

	inode->vnode.ref_count  = 1;
	inode->vnode.ops        = &proc_inode_ops;
	inode->vnode.superblock = superblock;
	inode->vnode.number     = (((uintptr_t)inode->proc) << 4) | inode->type;
	switch (type) {
	case INODE_SELF:
	case INODE_CWD:
	case INODE_EXE:
		inode->vnode.mode  = 0550;
		inode->vnode.flags = VFS_LINK;
		break;
	case INODE_MAPS:
	case INODE_CMDLINE:
	case INODE_STATUS:
		inode->vnode.mode  = 0444;
		inode->vnode.flags = VFS_FILE;
		break;
	case INODE_DIR:
	case INODE_FD_DIR:
		inode->vnode.mode  = 0550;
		inode->vnode.flags = VFS_DIR;
		break;
	}
	return &inode->vnode;
}

static int proc_readdir(vfs_node_t *vnode, unsigned long index, struct dirent *dirent) {
	proc_inode_t *inode = container_of(vnode, proc_inode_t, vnode);
	switch (inode->type) {
	case INODE_DIR:
		static char *content[] = {
			".",
			"..",
			"cwd",
			"maps",
			"cmdline",
			"exe",
			"fd",
			"status",
		};

		if (index >= sizeof(content) / sizeof(*content)) return -ENOENT;

		strcpy(dirent->d_name, content[index]);
		return 0;
	default:
		return -ENOSYS;
	}
}

static int proc_getattr(vfs_node_t *vnode, struct stat *st) {
	proc_inode_t *inode = container_of(vnode, proc_inode_t, vnode);
	st->st_uid          = inode->proc->euid;
	st->st_gid          = inode->proc->egid;
	return 0;
}

static ssize_t proc_readlink(vfs_node_t *vnode, char *buffer, size_t count) {
	proc_inode_t *inode = container_of(vnode, proc_inode_t, vnode);
	static char buf[64];
	switch (inode->type) {
	case INODE_CWD:;
		char *cwd = vfs_dentry_path(inode->proc->cwd);
		if (count > strlen(cwd) + 1) count = strlen(cwd) + 1;
		memcpy(buffer, cwd, count);
		kfree(cwd);
		break;
	case INODE_EXE:;
		char *exe = vfs_dentry_path(inode->proc->exe);
		if (count > strlen(exe) + 1) count = strlen(exe) + 1;
		memcpy(buffer, exe, count);
		kfree(exe);
		break;
	case INODE_SELF:
		sprintf(buf, "%ld", inode->proc->pid);
		if (count > strlen(buf) + 1) count = strlen(buf) + 1;
		memcpy(buffer, buf, count);
		break;
	default:
		return -EINVAL;
	}
	return count;
}

static int proc_lookup(vfs_node_t *vnode, vfs_dentry_t *dentry) {
	proc_inode_t *inode = container_of(vnode, proc_inode_t, vnode);

	if (!strcmp(dentry->name, "cwd")) {
		dentry->inode = proc_new_node(vnode->superblock, proc_ref(inode->proc), INODE_CWD);
		return 0;
	} else if (!strcmp(dentry->name, "maps")) {
		dentry->inode = proc_new_node(vnode->superblock, proc_ref(inode->proc), INODE_MAPS);
		return 0;
	} else if (!strcmp(dentry->name, "cmdline")) {
		dentry->inode = proc_new_node(vnode->superblock, proc_ref(inode->proc), INODE_CMDLINE);
		return 0;
	} else if (!strcmp(dentry->name, "exe")) {
		dentry->inode = proc_new_node(vnode->superblock, proc_ref(inode->proc), INODE_EXE);
		return 0;
	} else if (!strcmp(dentry->name, "fd")) {
		dentry->inode = proc_new_node(vnode->superblock, proc_ref(inode->proc), INODE_FD_DIR);
		return 0;
	} else if (!strcmp(dentry->name, "status")) {
		dentry->inode = proc_new_node(vnode->superblock, proc_ref(inode->proc), INODE_STATUS);
		return 0;
	}
	return -ENOENT;
}

static int proc_open(vfs_fd_t *fd) {
	fd->ops = &proc_fd_ops;
	return 0;
}

static ssize_t proc_read(vfs_fd_t *fd, void *buf, off_t offset, size_t count) {
	proc_inode_t *inode = container_of(fd->inode, proc_inode_t, vnode);
	char str_buf[4096];
	char *str;
	size_t i        = 0;
	process_t *proc = inode->proc;
	switch (inode->type) {
	case INODE_MAPS:
		foreach (node, &proc->vmm_space.segs) {
			vmm_seg_t *seg = (vmm_seg_t *)node;
			char prot[5];
			prot[0] = seg->prot & MMU_FLAG_READ ? 'r' : '-';
			prot[1] = seg->prot & MMU_FLAG_WRITE ? 'w' : '-';
			prot[2] = seg->prot & MMU_FLAG_EXEC ? 'x' : '-';
			prot[3] = seg->flags & VMM_FLAG_PRIVATE ? 'p' : 's';
			prot[4] = '\0';

			char *name = NULL;
			if (seg->flags & VMM_FLAG_ANONYMOUS) {
				name = strdup("[anonymous]");
			} else {
				name = vfs_dentry_path(seg->fd->dentry);
			}
			i += sprintf(str_buf + i, "%012lx-%012lx %s %6zd %s\n", seg->start, seg->end, prot, seg->offset, name);
			kfree(name);
		}
		str = str_buf;
		break;
	case INODE_STATUS:
		str = str_buf;
		sprintf(str_buf, "Pid: %ld\n"
						 "Ppid: %ld\n"
						 "VmSize: %zu\n"
						 "VmPeak: %zu\n"
						 "VmAnon: %zu\n"
						 "VmFile: %zu\n"
						 "VmPrivate: %zu\n"
						 "VmShared: %zu\n"
						 "PageFaults: %zu\n"
						 "VoluntaryContextSwitches: %zu\n"
						 "PreemptContextSwitches: %zu\n",
				proc->pid,
				proc->parent ? proc->parent->pid : proc->pid,
				proc->vmm_space.total_size,
				proc->vmm_space.peak_size,
				proc->vmm_space.anon_size,
				proc->vmm_space.file_size,
				proc->vmm_space.private_size,
				proc->vmm_space.shared_size,
				proc->vmm_space.page_faults,
				proc->main_thread->voluntary_context_switches,
				proc->main_thread->preempt_context_switches);
		break;
	case INODE_CMDLINE:
		str = proc->cmdline;
		break;
	default:
		return -EINVAL;
	}
	if ((size_t)offset >= strlen(str)) return 0;
	if (offset + count > strlen(str)) count = strlen(str) - offset;
	memcpy(buf, str, count);
	return count;
}

static void proc_cleanup(vfs_node_t *vnode) {
	proc_inode_t *inode = container_of(vnode, proc_inode_t, vnode);
	proc_release(inode->proc);
	kfree(inode);
}

static vfs_inode_ops_t proc_inode_ops = {
	.lookup   = proc_lookup,
	.readdir  = proc_readdir,
	.readlink = proc_readlink,
	.getattr  = proc_getattr,
	.cleanup  = proc_cleanup,
	.open     = proc_open,
};

static vfs_fd_ops_t proc_fd_ops = {
	.read = proc_read,
};

static int proc_root_lookup(vfs_node_t *root, vfs_dentry_t *dentry) {
	if (!strcmp(dentry->name, "self")) {
		dentry->inode = proc_new_node(root->superblock, proc_ref(get_current_proc()), INODE_SELF);
		return 0;
	}
	char *end;
	pid_t pid = strtol(dentry->name, &end, 10);
	if (end == dentry->name) return -ENOENT;
	process_t *proc = pid2proc(pid);
	if (!proc) return -ENOENT;

	dentry->inode = proc_new_node(root->superblock, proc, INODE_DIR);
	return 0;
}

static int proc_root_readdir(vfs_node_t *root, unsigned long index, struct dirent *dirent) {
	(void)root;
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

	index -= 3;

	xarray_foreach (pid, value, get_procs_list()) {
		(void)value;
		if (index == 0) {
			sprintf(dirent->d_name, "%d", pid);
			return 0;
		}
		index--;
	}
	return -ENOENT;
}

static vfs_inode_ops_t proc_root_ops = {
	.readdir = proc_root_readdir,
	.lookup  = proc_root_lookup,
};

int proc_mount(const char *source, const char *target, unsigned long flags, const void *data, vfs_superblock_t **superblock_out) {
	(void)data;
	(void)source;
	(void)flags;
	(void)target;

	vfs_node_t *vnode = kmalloc(sizeof(vfs_node_t));
	memset(vnode, 0, sizeof(vfs_node_t));
	vnode->flags     = VFS_DIR;
	vnode->ops       = &proc_root_ops;
	vnode->ref_count = 1;
	vnode->mode      = 0555;
	vnode->uid       = EUID_ROOT;
	vnode->gid       = EUID_ROOT;

	vfs_superblock_t *superblock = kmalloc(sizeof(vfs_superblock_t));
	memset(superblock, 0, sizeof(vfs_superblock_t));
	superblock->root = vnode;
	superblock->flags |= VFS_SUPERBLOCK_NO_DCACHE;
	vnode->superblock = superblock;

	*superblock_out = superblock;
	return 0;
}

vfs_filesystem_t proc_fs = {
	.name  = "proc",
	.mount = proc_mount,
};

void init_proc(void) {
	kdebugf("init proc fs ...");
	vfs_register_fs(&proc_fs);
	kok();
}
