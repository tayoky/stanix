#include <kernel/device.h>
#include <kernel/poll.h>
#include <kernel/slab.h>
#include <kernel/vfs.h>
#include <kernel/vmm.h>

static slab_cache_t fd_slab;

static int fd_constructor(slab_cache_t *cache, void *data) {
	(void)cache;
	vfs_fd_t *fd = data;
	memset(fd, 0, sizeof(vfs_fd_t));
	fd->ref_count = 1;
	return 0;
}

void init_vfs_fd(void) {
	slab_init(&fd_slab, sizeof(vfs_fd_t), "vfs-file-descriptors");
	fd_slab.constructor = fd_constructor;
}

ssize_t vfs_read(vfs_fd_t *fd, void *buffer, uint64_t offset, size_t count) {
	if (fd->type == S_IFBLK || fd->type == S_IFCHR) {
		// check if device is unplugged
		if (((device_t *)fd->private)->type == DEVICE_UNPLUGGED) return -ENODEV;
	} else if (fd->type == S_IFDIR) {
		return -EISDIR;
	}
	if (fd->flags & O_WRONLY) {
		return -EBADF;
	}

	if (fd->ops->read) {
		vfs_update_time(fd->inode, VNODE_ATTR_ATIME);
		return fd->ops->read(fd, buffer, offset, count);
	} else {
		return -EINVAL;
	}
}

ssize_t vfs_write(vfs_fd_t *fd, const void *buffer, uint64_t offset, size_t count) {
	if (fd->type == S_IFBLK || fd->type == S_IFCHR) {
		// check if device is unplugged
		if (((device_t *)fd->private)->type == DEVICE_UNPLUGGED) return -ENODEV;
	} else if (fd->type == S_IFDIR) {
		return -EISDIR;
	}
	if (!(fd->flags & (O_WRONLY | O_RDWR))) {
		return -EBADF;
	}
	if (fd->ops->write) {
		vfs_update_time(fd->inode, VNODE_ATTR_MTIME);
		return fd->ops->write(fd, buffer, offset, count);
	} else {
		return -EINVAL;
	}
}

int vfs_ioctl(vfs_fd_t *fd, long request, void *arg) {
	if (!fd || !fd->ops->ioctl) return -EBADF;
	if (fd->type == S_IFBLK|| fd->type == S_IFCHR) {
		// check if device is unplugged
		if (((device_t *)fd->private)->type == DEVICE_UNPLUGGED) return -ENODEV;
	}
	return fd->ops->ioctl(fd, request, arg);
}

int vfs_poll_add(vfs_fd_t *fd, poll_event_t *event) {
	if (!fd) return -EBADF;
	if (!fd->ops->poll_add) {
		// by default files are always readable and writable
		event->revents = POLLIN | POLLOUT;
		return 0;
	}
	return fd->ops->poll_add(fd, event);
}

int vfs_poll_remove(vfs_fd_t *fd, poll_event_t *event) {
	if (!fd) return -EBADF;
	if (!fd->ops->poll_remove) return 0;
	return fd->ops->poll_remove(fd, event);
}

int vfs_poll_get(vfs_fd_t *fd, poll_event_t *event) {
	if (!fd) return -EBADF;
	if (!fd->ops->poll_get) {
		// by default files are always readable and writable
		event->revents = POLLIN | POLLOUT;
		return 0;
	}
	event->revents = 0;
	int ret        = fd->ops->poll_get(fd, event);
	// cap events
	event->revents &= event->events | POLLHUP | POLLNVAL | POLLHUP;
	return ret;
}

vfs_fd_t *vfs_open_at(vfs_dentry_t *at, const char *path, long flags, ...) {
	mode_t mode = 0777;
	if (flags & O_CREAT) {
		va_list args;
		va_start(args, flags);
		mode = va_arg(args, mode_t);
		va_end(args);
	}

	vfs_dentry_t *dentry = vfs_get_dentry_at(at, path, flags, mode);
	if (IS_ERR(dentry)) return (vfs_fd_t *)dentry;

	vfs_fd_t *fd = vfs_open_node(dentry->inode, dentry, flags);
	vfs_dentry_release(dentry);
	return fd;
}

vfs_fd_t *vfs_fd_alloc(void) {
	return slab_alloc(&fd_slab);
}

vfs_fd_t *vfs_open_node(vfs_node_t *node, vfs_dentry_t *dentry, long flags) {
	// permission checking
	mode_t required_perm = PERM_READ;
	if (flags & O_RDWR) {
		required_perm = PERM_READ | PERM_WRITE;
	} else if (flags & O_WRONLY) {
		required_perm = PERM_WRITE;
	}
	if ((vfs_perm(node) & required_perm) != required_perm) {
		return ERR2PTR(-EACCES);
	}

	vfs_fd_t *fd = vfs_fd_alloc();
	if (!fd) return ERR2PTR(-ENOMEM);
	struct stat st;
	vfs_getattr(node, &st);

	fd->ops     = NULL;
	fd->private = node->private_inode;
	fd->inode   = vfs_node_ref(node);
	fd->dentry  = vfs_dentry_ref(dentry);
	fd->flags   = flags;
	fd->type    = node->mode & S_IFMT;

	// call inode specific open before fd specific open
	if (node->ops && node->ops->open) {
		node->ops->open(fd);
	}

	int ret = 0;
	if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)) {
		device_t *device = device_from_number(st.st_rdev);
		if (!device) {
			ret = -ENODEV;
			goto error;
		}
		fd->ops     = device->ops;
		fd->private = device;
	}

	if (fd->ops && fd->ops->open) {
		ret = fd->ops->open(fd);
		if (ret < 0) {
error:
			vfs_node_release(fd->inode);
			slab_free(fd);
			return ERR2PTR(ret);
		}
	}


	/// update modify / access time
	if (flags & O_RDWR) {
		vfs_update_time(node, VNODE_ATTR_ATIME | VNODE_ATTR_MTIME);
	} else if (flags & O_WRONLY) {
		vfs_update_time(node, VNODE_ATTR_MTIME);
	} else {
		vfs_update_time(node, VNODE_ATTR_ATIME);
	}

	return fd;
}

void vfs_close(vfs_fd_t *fd) {
	if (!fd) return;
	if (ref_count_dec(&fd->ref_count) > 1) {
		return;
	}

	if (fd->ops && fd->ops->close) {
		fd->ops->close(fd);
	}

	if (fd->type == S_IFBLK || fd->type == S_IFCHR) {
		device_release(fd->private);
	}
	vfs_node_release(fd->inode);
	vfs_dentry_release(fd->dentry);

	slab_free(fd);
}
