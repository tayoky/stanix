#ifndef TTY_H
#define TTY_H

#include <kernel/vfs.h>
#include <kernel/ringbuf.h>
#include <kernel/list.h>
#include <kernel/scheduler.h>
#include <termios.h>
#include <sys/ioctl.h>

typedef struct tty {
	void *private_data;
	void (*out)(char,void *);
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
	vfs_node *slave;
} pty_t;

/// @brief give a char to the input of a tty
/// @param tty 
/// @param c 
/// @return 
int tty_input(tty_t *tty,char c);

int tty_output(tty_t *tty,char c);

/// @brief create a new tty
/// @param tyy 
/// @return ane vfs_node that represent the tty
vfs_node *new_tty(tty_t **tty);


int new_pty(vfs_node **master,vfs_node **slave,tty_t **);

ssize_t tty_read(vfs_node *node,void *buffer,uint64_t offset,size_t count);
ssize_t tty_write(vfs_node *node,void *buffer,uint64_t offset,size_t count);
int tty_wait_check(vfs_node *node,short type);

#endif