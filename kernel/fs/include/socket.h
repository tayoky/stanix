#ifndef _KERNEL_SOCKET_H
#define _KERNEL_SOCKET_H

#include <kernel/vfs.h>
#include <sys/socket.h>

struct socket_domain;

typedef struct socket {
	vfs_node node;
	struct socket_domain *domain;
	int type;
	int protocol;
	ssize_t (*sendmsg)(struct socket *socket, const struct msghdr *message, int flags);
	ssize_t (*recvmsg)(struct socket *socket, struct msghdr *message, int flags);
	int (*accept)(struct socket *socket, struct sockaddr *address, socklen_t *address_len, struct socket **new_sock);
	int (*bind)(struct socket *socket, const struct sockaddr *address, socklen_t address_len);
	int (*connect)(struct socket *socket, const struct sockaddr *address, socklen_t address_len);
	int (*listen)(struct socket *socket, int backlog);
	struct sockaddr *connected;
	socklen_t connected_len;
} socket_t;

typedef struct socket_domain {
	const char *name;
	int domain;
	socket_t *(*create)(int type, int protocol);
} socket_domain_t;

void init_sockets(void);
vfs_fd_t *create_socket(int domain, int type, int protocol);
void *socket_new(size_t size);
void register_socket_domain(socket_domain_t *domain);
void unregister_socket_domain(socket_domain_t *domain);


ssize_t socket_sendmsg(vfs_fd_t *socket, const struct msghdr *message, int flags);
ssize_t socket_recvmsg(vfs_fd_t *socket, struct msghdr *message, int flags);
int socket_accept(vfs_fd_t *socket, struct sockaddr *address, socklen_t *address_len, vfs_fd_t **new_sock);
int socket_bind(vfs_fd_t *socket, const struct sockaddr *address, socklen_t address_len);
int socket_connect(vfs_fd_t *socket, const struct sockaddr *address, socklen_t address_len);
int socket_listen(vfs_fd_t *socket, int backlog);

#endif