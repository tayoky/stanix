#include <kernel/socket.h>
#include <kernel/unix.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/print.h>
#include <kernel/scheduler.h>
#include <kernel/ringbuf.h>
#include <errno.h>

#define QUEUE_SIZE 4096

extern socket_domain_t unix_domain;
socket_t *unix_create(int type, int protocol);

/**
 * @brief pair two socket
 */
static void unix_pair(unix_socket_t *a, unix_socket_t *b) {
	a->status = UNIX_STATUS_CONNECTED;
	b->status = UNIX_STATUS_CONNECTED;
	a->connected = b;
	b->connected = a;

	// init the recieve buffers
	a->queue = new_ringbuffer(QUEUE_SIZE);
	b->queue = new_ringbuffer(QUEUE_SIZE);
}

int unix_bind(socket_t *sock, const struct sockaddr *addr, socklen_t addr_len) {
	struct sockaddr_un *address = (struct sockaddr_un*)addr;
	unix_socket_t *socket = (unix_socket_t*)sock;

	if (addr_len != sizeof(struct sockaddr_un) || address->sun_family != AF_UNIX) return -EINVAL;
	if (socket->status != UNIX_STATUS_INIT) return -EINVAL;

	int ret = vfs_create_ext(address->sun_path, 0777 & ~get_current_proc()->umask, VFS_SOCK, sock);
	if (ret < 0) {
		if (ret == -EEXIST) ret = -EADDRINUSE;
		return ret;
	}

	socket->bound = *address;
	socket->status = UNIX_STATUS_BOUND;
	return 0;
}


int unix_connect(socket_t *sock, const struct sockaddr *addr, socklen_t addr_len) {
	struct sockaddr_un *address = (struct sockaddr_un*)addr;
	unix_socket_t *socket = (unix_socket_t*)sock;

	if (addr_len != sizeof(struct sockaddr_un) || address->sun_family != AF_UNIX) return -EINVAL;
	if (socket->status != UNIX_STATUS_INIT) return -EINVAL;

	vfs_node *node = vfs_open(address->sun_path, VFS_READWRITE);
	unix_socket_t *server = (unix_socket_t*)node;
	if (!node) return -ECONNREFUSED;
	if (!(node->flags & VFS_SOCK) || (server->status != UNIX_STATUS_LISTEN) || 
		server->socket.type != sock->type || server->socket.domain != sock->domain) return -ECONNREFUSED;

	// send the connection struct
	unix_connection_t connection = {
		.socket = socket,
	};

	// ringbuf write can fail (syscall interrupted/server socket dies before accepting/...)
	ssize_t ret = ringbuffer_write(&connection, server->queue, sizeof(unix_connection_t));
	if (ret < 0) return ret;

	// FIXME : if we get interrupted here we will still be in the connect queue
	int r = sleep_on_queue(&socket->sleep);
	if (r < 0) return r;

	return socket->status == UNIX_STATUS_CONNECTED ? 0 : -ECONNREFUSED;
}

int unix_listen(socket_t *sock, int backlog) {
	unix_socket_t *socket = (unix_socket_t*)sock;
	if (socket->status == UNIX_STATUS_INIT) return -EDESTADDRREQ;
	if (socket->status != UNIX_STATUS_BOUND) return -EINVAL;

	socket->queue = new_ringbuffer(sizeof(unix_connection_t) * backlog);
	socket->status = UNIX_STATUS_LISTEN;
	return 0;
}


int unix_accept(socket_t *sock, struct sockaddr *addr, socklen_t *addr_len, socket_t **new_sock) {
	struct sockaddr_un *address = (struct sockaddr_un*)addr;
	unix_socket_t *socket = (unix_socket_t*)sock;

	if (socket->status != UNIX_STATUS_LISTEN) return -EINVAL;

	unix_connection_t connection;

	// ringbuf write can fail (syscall interrupted/...)
	ssize_t ret = ringbuffer_read(&connection, socket->queue, sizeof(unix_connection_t));
	if (ret < 0) return ret;

	// we can now connect to the socket
	*new_sock = unix_create(sock->type, sock->protocol);
	unix_pair((unix_socket_t*)*new_sock, connection.socket);
	if (connection.socket->bound.sun_path[0]) {
		// the connected socket have an address
		*addr_len = sizeof(struct sockaddr_un);
		*address = connection.socket->bound;
	} else {
		// the connected socket don't have an bound address
		*addr_len = 0;
		*address = connection.socket->bound;
	}

	// wakeup the client sock
	wakeup_queue(&socket->sleep, 1);

	return 0;
}

ssize_t unix_recvmsg(socket_t *sock, struct msghdr *message, int flags) {
	(void)flags;
	unix_socket_t *socket = (unix_socket_t*)sock;

	ssize_t total = 0;

	if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
		// TODO
		return -ENOSYS;
	} else {
		if (socket->status != UNIX_STATUS_CONNECTED && socket->status != UNIX_STATUS_DISCONNECTED) return -ENOTCONN;
		if (message->msg_name) return -EISCONN;
		if (socket->status == UNIX_STATUS_DISCONNECTED && !ringbuffer_read_available(socket->queue)) {
			// disconnected and nothing to read, we will never use this socket again
			return -ENOTCONN;
		}

		for (int i=0; i<message->msg_iovlen; i++) {
			ssize_t ret = ringbuffer_read(message->msg_iov[i].iov_base, socket->queue, message->msg_iov[i].iov_len);
			if (ret < 0) return ret;
			total += ret;
		}
	}

	return total;
}

ssize_t unix_sendmsg(socket_t *sock, const struct msghdr *message, int flags) {
	(void)flags;
	unix_socket_t *socket = (unix_socket_t*)sock;

	ssize_t total = 0;

	if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
		// TODO
		return -ENOSYS;
	} else {
		if (socket->status != UNIX_STATUS_CONNECTED) return -ENOTCONN;
		if (message->msg_name) return -EISCONN;

		for (int i=0; i<message->msg_iovlen; i++) {
			ssize_t ret = ringbuffer_write(message->msg_iov[i].iov_base, socket->connected->queue, message->msg_iov[i].iov_len);
			if (ret < 0) return ret;
			total += ret;
		}
	}
	return total;
}


void unix_close(vfs_node *node) {
	unix_socket_t *socket = (unix_socket_t*)node;
	kdebugf("unix cleanup\n");
	switch (socket->status) {
	case UNIX_STATUS_DISCONNECTED:
		delete_ringbuffer(socket->queue);
		break;
	case UNIX_STATUS_CONNECTED:
		// FIXME : we need some kind of lock
		// disconnect the peer
		socket->connected->status = UNIX_STATUS_DISCONNECTED;
		delete_ringbuffer(socket->queue);
		break;
	case UNIX_STATUS_LISTEN:
		delete_ringbuffer(socket->queue);
		if (socket->socket.type == SOCK_STREAM || socket->socket.type == SOCK_STREAM) {
			wakeup_queue(&socket->sleep, 0);
		}
		// fallthrough
	case UNIX_STATUS_BOUND:
		vfs_unlink(socket->bound.sun_path);
		break;
	}
}

socket_t *unix_create(int type, int protocol) {
	(void)protocol;
	if (type > SOCK_SEQPACKET) return NULL;

	unix_socket_t *socket = socket_new(sizeof(unix_socket_t));
	socket->socket.type     = type;
	socket->socket.protocol = protocol;
	socket->socket.domain   = &unix_domain;
	socket->status = UNIX_STATUS_INIT;
	socket->bound.sun_family = AF_UNIX;
	
	socket->socket.node.close = unix_close;
	socket->socket.accept  = unix_accept;
	socket->socket.bind    = unix_bind;
	socket->socket.connect = unix_connect;
	socket->socket.listen  = unix_listen;
	socket->socket.recvmsg = unix_recvmsg;
	socket->socket.sendmsg = unix_sendmsg;

	return (socket_t*)socket;
}

socket_domain_t unix_domain = {
	.name = "unix",
	.domain = AF_UNIX,
	.create = unix_create,
};

void init_unix_socket(void) {
	register_socket_domain(&unix_domain);
}
