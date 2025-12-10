#ifndef _KERNEL_UNIX_H
#define _KERNEL_UNIX_H

#include <kernel/socket.h>
#include <kernel/ringbuf.h>
#include <sys/un.h>

typedef struct unix_socket {
	socket_t socket;
	ring_buffer *queue;
	struct unix_socket *connected;
	int status;
	struct sockaddr_un bound;
} unix_socket_t;

typedef struct unix_connection {
	unix_socket_t *socket;
} unix_connection_t;

#define UNIX_STATUS_INIT         0
#define UNIX_STATUS_BOUND        1
#define UNIX_STATUS_LISTEN       2
#define UNIX_STATUS_CONNECTED    3
#define UNIX_STATUS_DISCONNECTED 4


void init_unix_socket(void);

#endif