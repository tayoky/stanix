#include <kernel/socket.h>
#include <kernel/kheap.h>
#include <kernel/list.h>
#include <kernel/string.h>
#include <kernel/poll.h>
#include <kernel/assert.h>
#include <errno.h>

static list_t socket_domains;

void init_sockets(void) {
	init_list(&socket_domains);
}

static ssize_t socket_read(vfs_fd_t *fd, void *buf, off_t offset, size_t count) {
	(void)offset;
	struct iovec vec = {
		.iov_base = buf,
		.iov_len  = count,
	};

	struct msghdr message = {
		.msg_iov = &vec,
		.msg_iovlen = 1,
	};

	return socket_recvmsg(fd, &message, 0);
}

static ssize_t socket_write(vfs_fd_t *fd, const void *buf, off_t offset, size_t count) {
	(void)offset;
	struct iovec vec = {
		.iov_base = (void *)buf,
		.iov_len  = count,
	};

	struct msghdr message = {
		.msg_iov = &vec,
		.msg_iovlen = 1,
	};

	return socket_sendmsg(fd, &message, 0);
}

static void socket_close(vfs_fd_t *fd) {
	socket_t *socket = container_of(fd, socket_t, fd);
	if (socket->domain->close) {
		socket->domain->close(socket);
	}
}

int socket_poll_add(vfs_fd_t *fd, poll_event_t *event) {
	socket_t *socket = container_of(fd, socket_t, fd);
	if (socket->type != VFS_SOCK) return -ENOTSOCK;
	if (!socket->domain->poll_add) return -EOPNOTSUPP;
	return socket->domain->poll_add(socket, event);
}

int socket_poll_remove(vfs_fd_t *fd, poll_event_t *event) {
	socket_t *socket = container_of(fd, socket_t, fd);
	if (socket->type != VFS_SOCK) return -ENOTSOCK;
	if (!socket->domain->poll_remove) return -EOPNOTSUPP;
	return socket->domain->poll_remove(socket, event);
}

int socket_poll_get(vfs_fd_t *fd, poll_event_t *event) {
	socket_t *socket = container_of(fd, socket_t, fd);
	if (socket->type != VFS_SOCK) return -ENOTSOCK;
	if (!socket->domain->poll_get) return -EOPNOTSUPP;
	return socket->domain->poll_get(socket, event);
}

static vfs_fd_ops_t socket_ops = {
	.poll_add    = socket_poll_add,
	.poll_remove = socket_poll_remove,
	.poll_get    = socket_poll_get,
	.read        = socket_read,
	.write       = socket_write,
	.close       = socket_close,
};

static void socket_init(socket_t *socket) {
	kassert(socket);
	kassert(socket->domain);
	socket->fd.ops     = &socket_ops;
	socket->fd.private = NULL;
	socket->fd.type    = VFS_SOCK;
	socket->fd.flags   = O_RDWR;
	socket->fd.ref_count = 1;
}

vfs_fd_t *socket_create(int domain, int type, int protocol) {
	foreach(node, &socket_domains) {
		socket_domain_t *cur_domain = (socket_domain_t *)node;
		if (cur_domain->domain != domain) {
			continue;
		}
		socket_t *socket = cur_domain->create(type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC), protocol);
		if (!socket) return NULL;
		socket_init(socket);
		if (type & SOCK_NONBLOCK) socket->fd.flags |= O_NONBLOCK;
		return &socket->fd;
	}

	return NULL;
}

ssize_t socket_sendmsg(vfs_fd_t *fd, const struct msghdr *message, int flags) {
	if (fd->type != VFS_SOCK) return -ENOTSOCK;
	socket_t *socket = container_of(fd, socket_t, fd);
	if (!socket->domain->sendmsg) return -EOPNOTSUPP;

	if ((socket->type == SOCK_RAW || socket->type == SOCK_DGRAM) && !message->msg_name) {
		if (!socket->connected) {
			return -EDESTADDRREQ;
		}
		struct msghdr dflt = *message;
		dflt.msg_name    = socket->connected;
		dflt.msg_namelen = socket->connected_len;
		message = &dflt;
		return socket->domain->sendmsg(socket, &dflt, flags);
	}

	return socket->domain->sendmsg(socket, message, flags);
}

ssize_t socket_recvmsg(vfs_fd_t *fd, struct msghdr *message, int flags) {
	if (fd->type != VFS_SOCK) return -ENOTSOCK;
	socket_t *socket = container_of(fd, socket_t, fd);
	if (!socket->domain->recvmsg) return -EOPNOTSUPP;

	return socket->domain->recvmsg(socket, message, flags);
}

int socket_accept(vfs_fd_t *fd, struct sockaddr *address, socklen_t *address_len, vfs_fd_t **new_sock_fd) {
	if (fd->type != VFS_SOCK) return -ENOTSOCK;
	socket_t *socket = container_of(fd, socket_t, fd);
	if (!socket->domain->accept || socket->type == SOCK_DGRAM || socket->type == SOCK_RAW) return -EOPNOTSUPP;

	socket_t *new_sock;
	uint32_t storage[128];
	socklen_t len_storage;

	if (!address) address = (struct sockaddr *)&storage;
	if (!address_len) address_len = &len_storage;

	int ret = socket->domain->accept(socket, address, address_len, &new_sock);
	if (ret >= 0) {
		socket_init(new_sock);
		*new_sock_fd = &new_sock->fd;

		// we propage non blocking socket
		if (socket->fd.flags & O_NONBLOCK) new_sock->fd.flags |= O_NONBLOCK;
	}
	return ret;
}

int socket_bind(vfs_fd_t *fd, const struct sockaddr *address, socklen_t address_len) {
	if (fd->type != VFS_SOCK) return -ENOTSOCK;
	socket_t *socket = container_of(fd, socket_t, fd);
	if (!socket->domain->bind) return -EOPNOTSUPP;

	return socket->domain->bind(socket, address, address_len);
}

int socket_connect(vfs_fd_t *fd, const struct sockaddr *address, socklen_t address_len) {
	if (fd->type != VFS_SOCK) return -ENOTSOCK;
	socket_t *socket = container_of(fd, socket_t, fd);
	if (!socket->domain->connect || socket->type == SOCK_DGRAM || socket->type == SOCK_RAW) {
		// standard connect that can work for datagram and raw sockets
		if ((int)address->sa_family != socket->domain->domain) return -EINVAL;
		kfree(socket->connected);
		socket->connected = kmalloc(address_len);
		*socket->connected = *address;
		socket->connected_len = address_len;
		return 0;
	}

	return socket->domain->connect(socket, address, address_len);
}

int socket_listen(vfs_fd_t *fd, int backlog) {
	if (fd->type != VFS_SOCK) return -ENOTSOCK;
	socket_t *socket = container_of(fd, socket_t, fd);
	if (!socket->domain->listen || socket->type == SOCK_DGRAM || socket->type == SOCK_RAW) return -EOPNOTSUPP;

	return socket->domain->listen(socket, backlog);
}

void socket_register_domain(socket_domain_t *domain) {
	list_append(&socket_domains, &domain->node);
}

void socket_unregister_domain(socket_domain_t *domain) {
	list_remove(&socket_domains, &domain->node);
}
