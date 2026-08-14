#ifndef KERNEL_SOCKET_H
#define KERNEL_SOCKET_H

#include <kernel/vfs.h>
#include <kernel/list.h>
#include <kernel/sleep.h>
#include <kernel/spinlock.h>
#include <abi/socket.h>

typedef struct socket socket_t;
typedef struct socket_packet socket_packet_t;
typedef struct socket_domain socket_domain_t;
struct poll_event;

struct socket {
	vfs_fd_t fd;
	list_t recived; // protected by lock
	sleep_queue_t sleep_queue;
	struct socket_domain *domain;
	int type;
	int protocol;
	int state;     // protected by lock
	spinlock_t lock;
};

#define SOCKET_STATE_INIT         0
#define SOCKET_STATE_LISTEN       1
#define SOCKET_STATE_CONNECTED    2
#define SOCKET_STATE_DISCONNECTED 3

struct socket_packet {
	list_node_t node;
	size_t size;
	void *data;
	size_t read;
};

struct socket_domain {
	list_node_t node;
	const char *name;
	int domain;
	socket_t *(*create)(int type, int protocol);
	ssize_t (*sendmsg)(socket_t *socket, const struct msghdr *message, int flags);
	ssize_t (*recvmsg)(socket_t *socket, struct msghdr *message, int flags);
	int (*accept)(socket_t *socket, struct sockaddr *address, socklen_t *address_len, socket_t **new_sock);
	int (*bind)(socket_t *socket, const struct sockaddr *address, socklen_t address_len);
	int (*connect)(socket_t *socket, const struct sockaddr *address, socklen_t address_len);
	int (*listen)(socket_t *socket, int backlog);
	int (*poll_add)(socket_t *, struct poll_event *);
	int (*poll_remove)(socket_t *, struct poll_event *);
	int (*poll_get)(socket_t *, struct poll_event *);
	void (*close)(socket_t *socket);
};

void init_sockets(void);
vfs_fd_t *socket_create(int domain, int type, int protocol);
void *socket_new(size_t size);
void socket_register_domain(socket_domain_t *domain);
void socket_unregister_domain(socket_domain_t *domain);

ssize_t socket_sendmsg(vfs_fd_t *socket, const struct msghdr *message, int flags);
ssize_t socket_recvmsg(vfs_fd_t *socket, struct msghdr *message, int flags);
int socket_accept(vfs_fd_t *socket, struct sockaddr *address, socklen_t *address_len, vfs_fd_t **new_sock);
int socket_bind(vfs_fd_t *socket, const struct sockaddr *address, socklen_t address_len);
int socket_connect(vfs_fd_t *socket, const struct sockaddr *address, socklen_t address_len);
int socket_listen(vfs_fd_t *socket, int backlog);

// function for the socket implementation
int socket_queue_recived_packet(socket_t *socket, void *data, size_t size);
ssize_t socket_dequeue_recived_packet(socket_t *socket, void *buf, size_t size, int keep_bounds);
int socket_queue_connection(socket_t *socket, void *data);
void *socket_dequeue_connection(socket_t *socket);
void socket_disconnect(socket_t *socket);

#endif
