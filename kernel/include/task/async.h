#ifndef _KERNEL_ASYNC_H
#define _KERNEL_ASYNC_H

typedef void (*async_callback_t)(void *arg);

typedef struct async {
	void *arg;
} async_t;

#endif
