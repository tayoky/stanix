#ifndef _KERNEL_TTY_H
#define _KERNEL_TTY_H

#include <kernel/vfs.h>
#include <kernel/ringbuf.h>
#include <kernel/list.h>
#include <kernel/scheduler.h>
#include <kernel/device.h>
#include <termios.h>
#include <sys/ioctl.h>

struct tty;

typedef struct tty_ops {
	int (* ioctl)(struct tty *, long , void *);
	ssize_t (* out)(struct tty *, const char *, size_t);
	void (* cleanup)(struct tty *);
} tty_ops_t;

typedef struct tty {
	device_t device;
	void *private_data;
	ring_buffer *input_buffer;
	tty_ops_t *ops;
	struct termios termios;
	struct winsize size;
	list_t *waiter;
	size_t column;
	char *canon_buf;
	size_t canon_index;
	pid_t fg_pgrp;
	int unconnected;
} tty_t;

typedef struct pty {
	ring_buffer *output_buffer;
	tty_t *slave;
} pty_t;

/// @brief give a char to the input of a tty
/// @param tty 
/// @param c 
/// @return 
int tty_input(tty_t *tty,char c);

int tty_output(tty_t *tty,char c);

/**
 * @brief create a new tty
 * @param tty the tty to init
 * @return the new tty
 */
tty_t *new_tty(tty_t *tty);


int new_pty(vfs_fd_t **master,vfs_fd_t **slave,tty_t **);

void init_ptys(void);

#endif
