#ifndef KERNEL_UNIX_H
#define KERNEL_UNIX_H

#include <kernel/socket.h>
#include <kernel/ringbuf.h>
#include <kernel/sleep.h>
#include <kernel/spinlock.h>
#include <sys/un.h>

typedef struct unix_socket {
	socket_t socket;
	ringbuffer_t queue;
	struct unix_socket *connected;
	sleep_queue_t sleep;
	spinlock_t lock;
} unix_socket_t;

typedef struct unix_connection {
	unix_socket_t *socket;
} unix_connection_t;

void init_unix_socket(void);

#endif
