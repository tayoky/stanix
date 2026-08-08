#ifndef KERNEL_TTY_H
#define KERNEL_TTY_H

#include <kernel/device.h>
#include <kernel/list.h>
#include <kernel/ringbuf.h>
#include <kernel/process.h>
#include <kernel/vfs.h>
#include <abi/ioctl.h>
#include <abi/termios.h>

struct tty;

typedef struct tty_ops {
	int (*ioctl)(struct tty *, long, void *);
	ssize_t (*out)(struct tty *, const char *, size_t);
	void (*cleanup)(struct tty *);
} tty_ops_t;

typedef struct tty {
	device_t device;
	void *private_data;
	ringbuffer_t input_buffer;
	tty_ops_t *ops;
	struct termios termios;
	struct winsize size;
	size_t column;
	char *canon_buf;
	size_t canon_index;
	process_group_t *fg_group;
} tty_t;

typedef struct pty {
	ringbuffer_t output_buffer;
	tty_t *slave;
} pty_t;

/**
 * @brief give a char to the input of a tty
 * @param tty
 * @param c
 * @return
 */
int tty_input(tty_t *tty, char c);

int tty_output(tty_t *tty, char c);

int tty_do_ioctl(tty_t *fd, long request, void *arg);


/**
 * @brief create a new tty
 * @param tty the tty to init
 * @return the new tty
 */
tty_t *new_tty(tty_t *tty);


int new_pty(vfs_fd_t **master, vfs_fd_t **slave, tty_t **);

void init_ptys(void);

#endif
