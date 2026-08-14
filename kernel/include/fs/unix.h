#ifndef KERNEL_UNIX_H
#define KERNEL_UNIX_H

#include <kernel/socket.h>
#include <kernel/ringbuf.h>
#include <kernel/sleep.h>
#include <kernel/spinlock.h>
#include <sys/un.h>

typedef struct unix_socket unix_socket_t;

struct unix_socket {
	socket_t socket;
	unix_socket_t *connected; // protected by socket.lock
	struct sockaddr_un addr;  // protected by socket.lock
	dev_t number;             // protected by socket.lock
};

void init_unix_socket(void);

#endif
