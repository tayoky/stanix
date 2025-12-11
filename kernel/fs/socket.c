#include <kernel/socket.h>
#include <kernel/kheap.h>
#include <kernel/list.h>
#include <kernel/string.h>
#include <errno.h>

static list_t *socket_domains;

void init_sockets(void) {
	socket_domains = new_list();
}

static ssize_t socket_read(vfs_node *node, void *buf, uint64_t offset, size_t count) {
	(void)offset;
	struct iovec vec = {
		.iov_base = buf,
		.iov_len  = count,
	};

	struct msghdr message = {
		.msg_iov = &vec,
		.msg_iovlen = 1,
	};

	return socket_recvmsg(node, &message, 0);
}


void *socket_new(size_t size) {
	socket_t *socket = kmalloc(size);
	memset(socket, 0, size);
	socket->node.ref_count = 1;
	socket->node.flags     = VFS_DEV | VFS_SOCK;
	socket->node.read      = socket_read;
	return socket;
}

vfs_node *create_socket(int domain, int type, int protocol) {
	foreach (node, socket_domains) {
		socket_domain_t *cur_domain = node->value;
		if (cur_domain->domain == domain) {
			return (vfs_node*)cur_domain->create(type, protocol);
		}
	}

	return NULL;
}

ssize_t socket_sendmsg(vfs_node *socket, const struct msghdr *message, int flags) {
	socket_t *sock = (socket_t *)sock;
	if (!(socket->flags & VFS_SOCK)) return -ENOTSOCK;
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

ssize_t socket_recvmsg(vfs_node *socket, struct msghdr *message, int flags) {
	socket_t *sock = (socket_t *)sock;
	if (!(socket->flags & VFS_SOCK)) return -ENOTSOCK;
	if (!sock->recvmsg) return -EOPNOTSUPP;

	return sock->recvmsg(sock, message, flags);
}

int socket_accept(vfs_node *socket, struct sockaddr *address, socklen_t *address_len, vfs_node **new_sock) {
	socket_t *sock = (socket_t *)sock;
	if (!(socket->flags & VFS_SOCK)) return -ENOTSOCK;
	if (!sock->accept || sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) return -EOPNOTSUPP;

	uint32_t storage[128];
	
	if (!address) address = (struct sockaddr *)&storage;

	return sock->accept(sock, address, address_len, (socket_t**)new_sock);
}

int socket_bind(vfs_node *socket, const struct sockaddr *address, socklen_t address_len) {
	socket_t *sock = (socket_t *)sock;
	if (!(socket->flags & VFS_SOCK)) return -ENOTSOCK;
	if (!sock->bind) return -EOPNOTSUPP;

	return sock->bind(sock, address, address_len);
}

int socket_connect(vfs_node *socket, const struct sockaddr *address, socklen_t address_len) {
	socket_t *sock = (socket_t *)sock;
	if (!(socket->flags & VFS_SOCK)) return -ENOTSOCK;
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

int socket_listen(vfs_node*socket, int backlog) {
	socket_t *sock = (socket_t *)sock;
	if (!(socket->flags & VFS_SOCK)) return -ENOTSOCK;
	if (!sock->listen || sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) return -EOPNOTSUPP;

	return sock->listen(sock, backlog);
}

void register_socket_domain(socket_domain_t *domain) {
	list_append(socket_domains, domain);
}

void unregister_socket_domain(socket_domain_t *domain) {
	list_remove(socket_domains, domain);
}