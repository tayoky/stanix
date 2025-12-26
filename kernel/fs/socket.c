#include <kernel/socket.h>
#include <kernel/kheap.h>
#include <kernel/list.h>
#include <kernel/string.h>
#include <errno.h>

static list_t *socket_domains;

void init_sockets(void) {
	socket_domains = new_list();
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

static void socket_close(vfs_fd_t *fd) {
	socket_t *socket = fd->private;
	if (socket->close) {
		socket->close(socket);
	}
}


int socket_wait_check(vfs_fd_t *socket, short type) {
	socket_t *sock = socket->private;
	if (!(socket->type & VFS_SOCK)) return -ENOTSOCK;
	if (!sock->wait_check) return -EOPNOTSUPP;
	return sock->wait_check(sock, type);
}

void *socket_new(size_t size) {
	socket_t *socket = kmalloc(size);
	memset(socket, 0, size);
	return socket;
}

static vfs_ops_t socket_ops = {
	.wait_check = socket_wait_check,
	.read       = socket_read,
	.close      = socket_close,
};

static vfs_fd_t *open_socket(socket_t *socket) {
	if (!socket) return NULL;
	vfs_fd_t *fd = kmalloc(sizeof(vfs_fd_t));
	memset(fd, 0, sizeof(vfs_fd_t));
	fd->ops       = &socket_ops;
	fd->private   = socket;
	fd->ref_count = 1;
	fd->type      = VFS_SOCK;
	fd->flags     = O_RDWR;
	return fd;
}

vfs_fd_t *create_socket(int domain, int type, int protocol) {
	foreach (node, socket_domains) {
		socket_domain_t *cur_domain = node->value;
		if (cur_domain->domain == domain) {
			return open_socket(cur_domain->create(type, protocol));
		}
	}

	return NULL;
}

ssize_t socket_sendmsg(vfs_fd_t *socket, const struct msghdr *message, int flags) {
	socket_t *sock = socket->private;
	if (!(socket->type & VFS_SOCK)) return -ENOTSOCK;
	if (!sock->sendmsg) return -EOPNOTSUPP;

	if ((sock->type == SOCK_RAW || sock->type == SOCK_DGRAM) && !message->msg_name) {
		if (!sock->connected) {
			return -EDESTADDRREQ;
		}
		struct msghdr dflt = *message;
		dflt.msg_name    = sock->connected;
		dflt.msg_namelen = sock->connected_len;
		message = &dflt;
		return sock->sendmsg(sock, &dflt, flags);
	}

	return sock->sendmsg(sock, message, flags);
}

ssize_t socket_recvmsg(vfs_fd_t *socket, struct msghdr *message, int flags) {
	socket_t *sock = (socket_t *)socket->private;
	if (!(socket->type & VFS_SOCK)) return -ENOTSOCK;
	if (!sock->recvmsg) return -EOPNOTSUPP;

	return sock->recvmsg(sock, message, flags);
}

int socket_accept(vfs_fd_t *socket, struct sockaddr *address, socklen_t *address_len, vfs_fd_t **new_sock_fd) {
	socket_t *sock = (socket_t *)socket->private;
	if (!(socket->type & VFS_SOCK)) return -ENOTSOCK;
	if (!sock->accept || sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) return -EOPNOTSUPP;
	
	socket_t *new_sock;
	uint32_t storage[128];
	socklen_t len_storage;
	
	if (!address) address = (struct sockaddr *)&storage;
	if (!address_len) address_len = &len_storage;

	int ret = sock->accept(sock, address, address_len, &new_sock);
	if (ret >= 0) {
		*new_sock_fd = open_socket(new_sock);
	}
	return ret;
}

int socket_bind(vfs_fd_t *socket, const struct sockaddr *address, socklen_t address_len) {
	socket_t *sock = (socket_t *)socket->private;
	if (!(socket->type & VFS_SOCK)) return -ENOTSOCK;
	if (!sock->bind) return -EOPNOTSUPP;

	return sock->bind(sock, address, address_len);
}

int socket_connect(vfs_fd_t *socket, const struct sockaddr *address, socklen_t address_len) {
	socket_t *sock = (socket_t *)socket->private;
	if (!(socket->type & VFS_SOCK)) return -ENOTSOCK;
	if (!sock->connect || sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
		// standard connect that can work for datagram and raw sockets
		if ((int)address->sa_family != sock->domain->domain) return -EINVAL;
		kfree(sock->connected);
		sock->connected = kmalloc(address_len);
		*sock->connected = *address;
		sock->connected_len = address_len;
		return 0;
	}

	return sock->connect(sock, address, address_len);
}

int socket_listen(vfs_fd_t *socket, int backlog) {
	socket_t *sock = (socket_t *)socket->private;
	if (!(socket->type & VFS_SOCK)) return -ENOTSOCK;
	if (!sock->listen || sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) return -EOPNOTSUPP;

	return sock->listen(sock, backlog);
}

void register_socket_domain(socket_domain_t *domain) {
	list_append(socket_domains, domain);
}

void unregister_socket_domain(socket_domain_t *domain) {
	list_remove(socket_domains, domain);
}