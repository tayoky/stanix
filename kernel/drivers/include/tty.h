#ifndef TTY_H
#define TTY_H

#include <kernel/vfs.h>
#include <kernel/ringbuf.h>
#include <kernel/list.h>
#include <kernel/scheduler.h>
#include <termios.h>

typedef struct tty {
	void *private_data;
	void (*out)(char,void *);
	ring_buffer input_buffer;
	struct termios termios;
	list *waiter;
	size_t column;
	char *canon_buf;
	size_t canon_index;
	process *fg_proc;
} tty;

/// @brief give a char to the input of a tty
/// @param tty 
/// @param c 
/// @return 
int tty_input(tty *tty,char c);

int tty_output(tty *tty,char c);

/// @brief create a new tty
/// @param tyy 
/// @return ane vfs_node that represent the tty
vfs_node *new_tty(tty **tyy);


int new_pty(vfs_node **master,vfs_node **slave);

ssize_t tty_read(vfs_node *node,void *buffer,uint64_t offset,size_t count);
ssize_t tty_write(vfs_node *node,void *buffer,uint64_t offset,size_t count);

#endif