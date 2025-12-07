#ifndef _KERNEL_SOCKET_H
#define _KERNEL_SOCKET_H

#include <kernel/vfs.h>
#include <sys/socket.h>

struct socket_domain;

typedef struct socket {
	vfs_node node;
	struct socket_domain *domain;
	int type;
	ssize_t (*sendmsg)(struct socket *socket, const struct msghdr *message, int flags);
	ssize_t (*recvmsg)(struct socket *socket, struct msghdr *message, int flags);
	struct socket *(*accept)(struct socket *socket, struct sockaddr *address, socklen_t *address_len);
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
socket_t *create_socket(int domain, int type, int protocol);
void register_socket_domain(socket_domain_t *domain);
void unregister_socket_domain(socket_domain_t *domain);

#endif