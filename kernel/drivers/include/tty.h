#ifndef TTY_H
#define TTY_H

#include <kernel/vfs.h>
#include <kernel/ringbuf.h>
#include <kernel/list.h>
#include <kernel/scheduler.h>
#include <kernel/device.h>
#include <termios.h>
#include <sys/ioctl.h>

typedef struct tty {
	device_t device;
	void *private_data;
	void (*out)(char,void *);
	void (*cleanup)(void *);
	ring_buffer *input_buffer;
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

ssize_t tty_read(vfs_fd_t *node,void *buffer,uint64_t offset,size_t count);
ssize_t tty_write(vfs_fd_t *node,void *buffer,uint64_t offset,size_t count);
int tty_wait_check(vfs_fd_t *node,short type);

#endif
