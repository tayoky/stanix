#include <kernel/socket.h>
#include <kernel/kheap.h>
#include <kernel/list.h>
#include <kernel/string.h>
#include <kernel/userspace.h>
#include <kernel/poll.h>
#include <kernel/slab.h>
#include <kernel/assert.h>
#include <sys/socket.h>
#include <errno.h>

static slab_cache_t socket_packets_slab;
static list_t socket_domains;

void init_sockets(void) {
	list_init(&socket_domains);
	slab_init(&socket_packets_slab, sizeof(socket_packet_t), "socket-packets");
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
	spinlock_acquire(&socket->lock);
	if (socket->domain->close) {
		socket->domain->close(socket);
	}
}

int socket_poll_add(vfs_fd_t *fd, poll_event_t *event) {
	socket_t *socket = container_of(fd, socket_t, fd);
	if (fd->type != S_IFSOCK) return -ENOTSOCK;
	if (!socket->domain->poll_add) return -EOPNOTSUPP;
	spinlock_acquire(&socket->lock);
	int ret = socket->domain->poll_add(socket, event);
	spinlock_release(&socket->lock);
	return ret;
}

int socket_poll_remove(vfs_fd_t *fd, poll_event_t *event) {
	socket_t *socket = container_of(fd, socket_t, fd);
	if (fd->type != S_IFSOCK) return -ENOTSOCK;
	if (!socket->domain->poll_remove) return -EOPNOTSUPP;
	spinlock_acquire(&socket->lock);
	int ret = socket->domain->poll_remove(socket, event);
	spinlock_release(&socket->lock);
	return ret;
}

int socket_poll_get(vfs_fd_t *fd, poll_event_t *event) {
	socket_t *socket = container_of(fd, socket_t, fd);
	if (fd->type != S_IFSOCK) return -ENOTSOCK;
	if (!socket->domain->poll_get) return -EOPNOTSUPP;
	spinlock_acquire(&socket->lock);
	int ret = socket->domain->poll_get(socket, event);
	spinlock_release(&socket->lock);
	return ret;
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
	socket->fd.type    = S_IFSOCK;
	socket->fd.flags   = O_RDWR;
	socket->fd.ref_count = 1;
	socket->fd.inode = NULL;
	list_init(&socket->recived);
	socket->state = SOCKET_STATE_INIT;
}

int socket_queue_recived_packet(socket_t *socket, void *data, size_t size) {
	spinlock_assert_acquired(&socket->lock);
	spinlock_release(&socket->lock);
	socket_packet_t *packet = slab_alloc(&socket_packets_slab);
	spinlock_acquire(&socket->lock);
	int ret = 0;
	if (!packet) return -ENOMEM;
	packet->data = kmalloc(size);
	packet->size = size;
	packet->read = 0;
	if (!packet->data) {
		ret = -ENOMEM;
		goto free_packet;
	}

	if (safe_copy_from(packet->data, data, size) < 0) {
		ret = -EFAULT;
		kfree(packet->data);
free_packet:
		slab_free(packet);
		return ret;
	}

	kdebugf("queue packet\n");
	list_append(&socket->recived, &packet->node);
	wakeup_queue(&socket->sleep_queue, 0);
	return 0;
}

static int socket_wait(socket_t *socket) {
	spinlock_assert_acquired(&socket->lock);
	if (!list_is_empty(&socket->recived)) {
		// we have something
		return 0;
	}
	if (socket->fd.flags & O_NONBLOCK) {
		// non blocking socket
		return -EAGAIN;
	}

	int ret = sleep_on_queue_lock_interruptible(&socket->sleep_queue, &socket->lock, !list_is_empty(&socket->recived) || socket->state == SOCKET_STATE_DISCONNECTED);
	if (list_is_empty(&socket->recived) && socket->state == SOCKET_STATE_DISCONNECTED) ret = -ENOTCONN;
	if (ret < 0) return ret;
	
	return 0;
}

ssize_t socket_dequeue_recived_packet(socket_t *socket, void *buf, size_t size, int keep_bounds) {
	spinlock_assert_acquired(&socket->lock);
	int ret = socket_wait(socket);
	if (ret < 0) return ret;

	ssize_t total = 0;
	char *buffer = buf;
	while (size > 0 && !list_is_empty(&socket->recived)) {
		socket_packet_t *packet = container_of(socket->recived.first_node, socket_packet_t, node);

		size_t available = packet->size - packet->read;
		size_t to_read = available < size ? available : size;

		if (safe_copy_to(buffer, packet->data + packet->read, to_read) < 0) {
			return total > 0 ? total : -EFAULT;
		}
		
		packet->read += to_read;
		total        += to_read;
		buffer       += to_read;
		size         -= to_read;
		if (packet->read >= packet->size) {
			kdebugf("dequed packet\n");
			// whole packet is read
			list_remove(&socket->recived, &packet->node);
			kfree(packet->data);
			slab_free(packet);
		}
		if (keep_bounds) break;
	}
	return total;
}

int socket_queue_connection(socket_t *socket, void *data) {
	spinlock_assert_acquired(&socket->lock);
	spinlock_release(&socket->lock);
	socket_packet_t *packet = slab_alloc(&socket_packets_slab);
	spinlock_acquire(&socket->lock);
	if (!packet) return -ENOMEM;
	packet->data = data;
	
	list_append(&socket->recived, &packet->node);
	wakeup_queue(&socket->sleep_queue, 0);
	return 0;
}

void *socket_dequeue_connection(socket_t *socket) {
	spinlock_assert_acquired(&socket->lock);
	kassert(socket->state == SOCKET_STATE_LISTEN || socket->state == SOCKET_STATE_DISCONNECTED);
	int ret = socket_wait(socket);
	if (ret < 0)  {
		spinlock_release(&socket->lock);
		return ERR2PTR(ret);
	}
	
	socket_packet_t *packet = container_of(socket->recived.first_node, socket_packet_t, node);
	list_remove(&socket->recived, &packet->node);

	void *data = packet->data;
	slab_free(packet);
	return data;
}

void socket_set_state(socket_t *socket, int state) {
	spinlock_assert_acquired(&socket->lock);
	socket->state = state;
	wakeup_queue(&socket->sleep_queue, 0);
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

static ssize_t socket_raw_sendmsg(socket_t *socket, const struct msghdr *message, int flags) {
	if (!socket->domain->sendmsg) return -EOPNOTSUPP;

	if (socket->state == SOCKET_STATE_CONNECTED) {
		if (message->msg_name) {
			return -EISCONN;
		}
	} else if (socket->type == SOCK_STREAM || socket->type == SOCK_SEQPACKET) {
		return -EINVAL;
	} else if (!message->msg_name) {
		return -EDESTADDRREQ;
	}

	return socket->domain->sendmsg(socket, message, flags);
}

ssize_t socket_sendmsg(vfs_fd_t *fd, const struct msghdr *message, int flags) {
	if (fd->type != S_IFSOCK) return -ENOTSOCK;
	socket_t *socket = container_of(fd, socket_t, fd);
	spinlock_acquire(&socket->lock);
	ssize_t ret = socket_raw_sendmsg(socket, message, flags);
	spinlock_release(&socket->lock);
	return ret;
}

static ssize_t socket_raw_recvmsg(socket_t *socket, struct msghdr *message, int flags) {
	if (!socket->domain->recvmsg) return -EOPNOTSUPP;
	if ((socket->type == SOCK_STREAM || socket->type ==  SOCK_SEQPACKET) && socket->state != SOCKET_STATE_CONNECTED && socket->state != SOCKET_STATE_DISCONNECTED) return -EINVAL;

	return socket->domain->recvmsg(socket, message, flags);
}

ssize_t socket_recvmsg(vfs_fd_t *fd, struct msghdr *message, int flags) {
	if (fd->type != S_IFSOCK) return -ENOTSOCK;
	socket_t *socket = container_of(fd, socket_t, fd);
	spinlock_acquire(&socket->lock);
	ssize_t ret = socket_raw_recvmsg(socket, message, flags);
	spinlock_release(&socket->lock);
	return ret;
}

static int socket_raw_accept(socket_t *socket, struct sockaddr *address, socklen_t *address_len, vfs_fd_t **new_sock_fd) {
	if (!socket->domain->accept || socket->type == SOCK_DGRAM || socket->type == SOCK_RAW) return -EOPNOTSUPP;
	if (socket->state != SOCKET_STATE_LISTEN) return -EINVAL;

	socket_t *new_sock;

	int ret = socket->domain->accept(socket, address, address_len, &new_sock);
	if (ret >= 0) {
		kdebugf("accepted connection\n");
		socket_init(new_sock);
		new_sock->state = SOCKET_STATE_CONNECTED;
		*new_sock_fd = &new_sock->fd;

		// we propagate non blocking socket
		if (socket->fd.flags & O_NONBLOCK) new_sock->fd.flags |= O_NONBLOCK;
	}
	return ret;
}

int socket_accept(vfs_fd_t *fd, struct sockaddr *address, socklen_t *address_len, vfs_fd_t **new_sock_fd) {
	if (fd->type != S_IFSOCK) return -ENOTSOCK;
	socket_t *socket = container_of(fd, socket_t, fd);
	spinlock_acquire(&socket->lock);
	int ret = socket_raw_accept(socket, address, address_len, new_sock_fd);
	spinlock_release(&socket->lock);
	return ret;
}

static int socket_raw_bind(socket_t *socket, const struct sockaddr *address, socklen_t address_len) {
	if (!socket->domain->bind) return -EOPNOTSUPP;
	// UNSAFE
	if ((int)address->sa_family != socket->domain->domain) return -EINVAL;
	if (socket->state != SOCKET_STATE_INIT) return -EINVAL;

	return socket->domain->bind(socket, address, address_len);
}

int socket_bind(vfs_fd_t *fd, const struct sockaddr *address, socklen_t address_len) {
	if (fd->type != S_IFSOCK) return -ENOTSOCK;
	socket_t *socket = container_of(fd, socket_t, fd);
	spinlock_acquire(&socket->lock);
	int ret = socket_raw_bind(socket, address, address_len);
	spinlock_release(&socket->lock);
	return ret;
}

// TODO : disconnect support
static int socket_raw_connect(socket_t *socket, const struct sockaddr *address, socklen_t address_len) {
	if (!socket->domain->connect) return -EOPNOTSUPP;
	if ((int)address->sa_family != socket->domain->domain) return -EINVAL;
	if (socket->state != SOCKET_STATE_INIT) return -EINVAL;

	int ret = socket->domain->connect(socket, address, address_len);
	if (ret >= 0) {
		socket_set_state(socket, SOCKET_STATE_CONNECTED);
	}
	return ret;
}

int socket_connect(vfs_fd_t *fd, const struct sockaddr *address, socklen_t address_len) {
	if (fd->type != S_IFSOCK) return -ENOTSOCK;
	socket_t *socket = container_of(fd, socket_t, fd);
	spinlock_acquire(&socket->lock);
	int ret = socket_raw_connect(socket, address, address_len);
	spinlock_release(&socket->lock);
	return ret;
}

static int socket_raw_listen(socket_t *socket, int backlog) {
	if (!socket->domain->listen || socket->type == SOCK_DGRAM || socket->type == SOCK_RAW) return -EOPNOTSUPP;
	if (socket->state != SOCKET_STATE_INIT) return -EINVAL;

	int ret = socket->domain->listen(socket, backlog);
	if (ret >= 0) {
		socket->state = SOCKET_STATE_LISTEN;
	}
	return ret;
}

int socket_listen(vfs_fd_t *fd, int backlog) {
	if (fd->type != S_IFSOCK) return -ENOTSOCK;
	socket_t *socket = container_of(fd, socket_t, fd);
	spinlock_acquire(&socket->lock);
	int ret = socket_raw_listen(socket, backlog);
	spinlock_release(&socket->lock);
	return ret;
}

void socket_register_domain(socket_domain_t *domain) {
	list_append(&socket_domains, &domain->node);
}

void socket_unregister_domain(socket_domain_t *domain) {
	list_remove(&socket_domains, &domain->node);
}
