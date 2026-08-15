#include <kernel/socket.h>
#include <kernel/unix.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/print.h>
#include <kernel/slab.h>
#include <kernel/scheduler.h>
#include <kernel/ringbuf.h>
#include <kernel/xarray.h>
#include <kernel/poll.h>
#include <errno.h>
#include <poll.h>

#define QUEUE_SIZE 4096

static socket_domain_t unix_domain;
static slab_cache_t unix_sockets_slab;
static xarray_t unix_binding;
static socket_t *unix_create(int type, int protocol);

/**
 * @brief pair two socket
 */
static void unix_pair(unix_socket_t *a, unix_socket_t *b) {
	a->connected = b;
	b->connected = a;
}

// TODO : double bind protection
static int unix_bind(socket_t *sock, const struct sockaddr *addr, socklen_t addr_len) {
	unix_socket_t *socket = container_of(sock, unix_socket_t, socket);
	struct sockaddr_un *address = (struct sockaddr_un*)addr;

	if (addr_len != sizeof(struct sockaddr_un) || address->sun_family != AF_UNIX) return -EINVAL;

	socket->number = xarray_allocate(&unix_binding, socket);
	int ret = vfs_mknod(address->sun_path, 0777 | S_IFSOCK, socket->number);
	if (ret < 0) {
		if (ret == -EEXIST) ret = -EADDRINUSE;
		return ret;
	}

	// UNSAFE
	socket->addr = *(struct sockaddr_un *)addr;

	return 0;
}

static int unix_connect(socket_t *sock, const struct sockaddr *addr, socklen_t addr_len) {
	struct sockaddr_un *address = (struct sockaddr_un*)addr;
	unix_socket_t *socket = container_of(sock, unix_socket_t, socket);

	if (addr_len != sizeof(struct sockaddr_un) || address->sun_family != AF_UNIX) {
		return -EINVAL;
	}

	vfs_node_t *node = vfs_get_node(address->sun_path, O_RDWR);
	if (IS_ERR(node)) {
		return -ECONNREFUSED;
	}
	struct stat stat;
	vfs_getattr(node, &stat);
	unix_socket_t *server = xarray_get(&unix_binding, stat.st_rdev);
	vfs_node_release(node);
	if (!S_ISSOCK(stat.st_mode) || (server->socket.state != SOCKET_STATE_LISTEN) || 
		server->socket.type != sock->type || server->socket.domain != sock->domain) {
		return -ECONNREFUSED;
	}
	
	int ret = socket_queue_connection(&server->socket, socket);
	if (ret < 0) return ret;

	// FIXME : if we get interrupted here we will still be in the connect queue
	spinlock_release(&socket->socket.lock);
	int r = sleep_on_queue_interruptible(&socket->socket.sleep_queue);
	spinlock_acquire(&socket->socket.lock);
	if (r < 0) return r;

	if (socket->connected) {
		return 0;
	} else {
		return -ECONNREFUSED;
	}
}

static int unix_listen(socket_t *sock, int backlog) {
	unix_socket_t *socket = container_of(sock, unix_socket_t, socket);
	(void)socket;
	(void)backlog;
	return 0;
}

static int unix_accept(socket_t *sock, struct sockaddr *addr, socklen_t *addr_len, socket_t **new_sock) {
	struct sockaddr_un *address = (struct sockaddr_un*)addr;
	unix_socket_t *socket = container_of(sock, unix_socket_t, socket);

	unix_socket_t *peer = socket_dequeue_connection(&socket->socket);
	if (IS_ERR(peer)) {
		return PTR2ERR(peer);
	}

	// we can now connect to the socket
	*new_sock = unix_create(sock->type, sock->protocol);

	unix_pair(container_of(new_sock, unix_socket_t, socket), peer);
	if (addr_len) *addr_len = sizeof(struct sockaddr_un);
	// UNSAFE
	if (address) *address = peer->addr;

	// wakeup the peer sock
	wakeup_queue(&peer->socket.sleep_queue, 0);

	return 0;
}

static ssize_t unix_recvmsg(socket_t *sock, struct msghdr *message, int flags) {
	(void)flags;
	unix_socket_t *socket = container_of(sock, unix_socket_t, socket);

	ssize_t total = 0;

	if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
		// TODO
		return -ENOSYS;
	} else {
		if (socket->socket.state == SOCKET_STATE_DISCONNECTED && list_is_empty(&socket->socket.recived)) {
			// disconnected and nothing to read, we will never use this socket again
			return -ENOTCONN;
		}

		for (int i=0; i<message->msg_iovlen; i++) {
			ssize_t ret = socket_dequeue_recived_packet(sock, message->msg_iov[i].iov_base, message->msg_iov[i].iov_len, socket->socket.type == SOCK_SEQPACKET);
			if (ret < 0) return ret;
			total += ret;
		}
	}

	return total;
}

static ssize_t unix_sendmsg(socket_t *sock, const struct msghdr *message, int flags) {
	(void)flags;
	unix_socket_t *socket = container_of(sock, unix_socket_t, socket);

	ssize_t total = 0;
	kdebugf("unix socket sendmsg\n");

	if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
		// TODO
		return -ENOSYS;
	} else {
		for (int i=0; i<message->msg_iovlen; i++) {
			int ret = socket_queue_recived_packet(sock, message->msg_iov[i].iov_base, message->msg_iov[i].iov_len);
			if (ret < 0) return ret;
			total += message->msg_iov[i].iov_len;
		}
	}
	return total;
}

static int unix_poll_add(socket_t *sock, poll_event_t *event) {
	unix_socket_t *socket = container_of(sock, unix_socket_t, socket);
	switch (socket->socket.state) {
	case SOCKET_STATE_DISCONNECTED:
		// you can't really wait for a disconnected socket to become ready
		break;
	case SOCKET_STATE_CONNECTED:
	case SOCKET_STATE_LISTEN:
		sleep_add_to_queue(&socket->socket.sleep_queue);
		break;
	}
	return 0;
}

static int unix_poll_remove(socket_t *sock, poll_event_t *event) {
	unix_socket_t *socket = container_of(sock, unix_socket_t, socket);
	switch (socket->socket.state) {
	case SOCKET_STATE_DISCONNECTED:
	case SOCKET_STATE_CONNECTED:
	case SOCKET_STATE_LISTEN:
		sleep_remove_from_queue(&socket->socket.sleep_queue);
		break;
	}
	return 0;
}

static int unix_poll_get(socket_t *sock, poll_event_t *event) {
	unix_socket_t *socket = container_of(sock, unix_socket_t, socket);
	switch (socket->socket.state) {
	case SOCKET_STATE_DISCONNECTED:
		event->revents |= POLLHUP;
		break;
	case SOCKET_STATE_CONNECTED:
	case SOCKET_STATE_LISTEN:
		event->revents |= POLLOUT;
		if (!list_is_empty(&socket->socket.recived)) {
			event->revents |= POLLIN;
		}
		break;
	}
	return 0;
}

static void unix_close(socket_t *sock) {
	unix_socket_t *socket = container_of(sock, unix_socket_t, socket);
	kdebugf("unix cleanup\n");

	unix_socket_t *peer = socket->connected;
	socket->connected = NULL;

	if (socket->addr.sun_path[0]) {
		xarray_clear(&unix_binding, socket->number);
	}

	if (peer) {
		// release the socket lock to make sure we cannot deadlock
		// FIXME : we have a RACE if two threads free the socket at the same time
		spinlock_release(&socket->socket.lock);
		spinlock_acquire(&peer->socket.lock);
		socket_set_state(&peer->socket, SOCKET_STATE_DISCONNECTED);
		peer->connected = NULL;
		spinlock_release(&peer->socket.lock);
		spinlock_acquire(&socket->socket.lock);
	}
	spinlock_release(&socket->socket.lock);
}

static socket_t *unix_create(int type, int protocol) {
	(void)protocol;
	if (type > SOCK_SEQPACKET) return NULL;

	unix_socket_t *socket = slab_alloc(&unix_sockets_slab);
	memset(socket, 0, sizeof(unix_socket_t));
	socket->socket.type     = type;
	socket->socket.protocol = protocol;
	socket->socket.domain   = &unix_domain;
	socket->addr.sun_family = AF_UNIX;

	return &socket->socket;
}

static socket_domain_t unix_domain = {
	.name = "unix",
	.domain = AF_UNIX,
	.create  = unix_create,
	.close   = unix_close,
	.accept  = unix_accept,
	.bind    = unix_bind,
	.connect = unix_connect,
	.listen  = unix_listen,
	.recvmsg = unix_recvmsg,
	.sendmsg = unix_sendmsg,
	.poll_add    = unix_poll_add,
	.poll_remove = unix_poll_remove,
	.poll_get    = unix_poll_get,
};

void init_unix_socket(void) {
	slab_init(&unix_sockets_slab, sizeof(unix_socket_t), "unix-sockets");
	socket_register_domain(&unix_domain);
}
