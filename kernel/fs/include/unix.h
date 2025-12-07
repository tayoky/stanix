#ifndef _KERNEL_UNIX_H
#define _KERNEL_UNIX_H

#include <kernel/socket.h>

typedef struct unix_socket {
	socket_t socket;
} unix_socket_t;

#endif