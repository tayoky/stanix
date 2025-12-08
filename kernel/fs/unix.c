#include <kernel/socket.h>
#include <kernel/unix.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/ringbuf.h>
#include <errno.h>

int unix_bind(socket_t *sock, const struct sockaddr *addr, socklen_t addr_len) {
	struct sockaddr_un *address = (struct sockaddr_un*)addr;
	unix_socket_t *socket = (unix_socket_t*)sock;

	if (addr_len != sizeof(struct sockaddr_un) || address->sun_family != AF_UNIX) return -EINVAL;
	if (socket->status != UNIX_STATUS_INIT) return -EINVAL;

	int ret = vfs_create(address->sun_path, 0777, VFS_FILE | VFS_DEV | VFS_SOCK);
	if (ret < 0) {
		if (ret == -EEXIST) ret = -EADDRINUSE;
		return ret;
	}
	vfs_mount(address->sun_path, (vfs_node*)sock);

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
	if (!node) return -ECONNREFUSED;
	if (!(node->flags & VFS_SOCK) || ((unix_socket_t*)node)->status != UNIX_STATUS_LISTEN) return -ECONNREFUSED;

	socket->connected = (unix_socket_t*)node;
	socket->status = UNIX_STATUS_CONNECTED;

	// TODO : send the connection struct

	return 0;
}

int unix_listen(socket_t *sock, int backlog) {
	unix_socket_t *socket = (unix_socket_t*)sock;
	
	if (socket->status != UNIX_STATUS_BOUND) return -EINVAL;

	socket->queue = new_ringbuffer(sizeof(unix_connection_t) * backlog);
	socket->status = UNIX_STATUS_LISTEN;
	return 0;
}

void unix_close(vfs_node *node) {
	unix_socket_t *socket = (unix_socket_t*)node;
	switch (socket->status) {
	case UNIX_STATUS_CONNECTED:
		vfs_close((vfs_node*)socket->connected);
		break;
	case UNIX_STATUS_LISTEN:
		delete_ringbuffer(socket->queue);
		// fallthrough
	case UNIX_STATUS_BOUND:
		vfs_unmount(socket->bound.sun_path);
		vfs_unlink(socket->bound.sun_path);
		break;
	}
}

socket_t *unix_create(int type, int protocol) {
	(void)protocol;
	if (type > SOCK_SEQPACKET) return NULL;

	unix_socket_t *socket = kmalloc(sizeof(unix_socket_t));
	socket->status = UNIX_STATUS_INIT;
	socket->socket.node.close = unix_close;
	socket->socket.bind = unix_bind;
	if (type == SOCK_DGRAM || type == SOCK_SEQPACKET) {
		socket->socket.connect = unix_connect;
		socket->socket.listen  = unix_listen;
	}

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