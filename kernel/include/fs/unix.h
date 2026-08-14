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
	ringbuffer_t queue;
	unix_socket_t *connected;
	sleep_queue_t sleep;
	spinlock_t lock;
	struct sockaddr_un addr;
};

typedef struct unix_connection {
	unix_socket_t *socket;
} unix_connection_t;

void init_unix_socket(void);

#endif
