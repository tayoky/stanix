#ifndef KERNEL_TTY_H
#define KERNEL_TTY_H

#include <kernel/device.h>
#include <kernel/list.h>
#include <kernel/ringbuf.h>
#include <kernel/sleep.h>
#include <kernel/process.h>
#include <kernel/vfs.h>
#include <abi/ioctl.h>
#include <abi/termios.h>

typedef struct tty tty_t;

typedef struct tty_ops {
	int (*ioctl)(tty_t *, long, void *);
	int (*update_termios)(tty_t *tty, struct termios *new);
	ssize_t (*out)(tty_t *, const char *, size_t);
	void (*cleanup)(tty_t *);
} tty_ops_t;

typedef struct tty {
	device_t device;
	ringbuffer_t input_buffer;  // protected by lock
	sleep_queue_t reader_queue; // protected by lock
	sleep_queue_t writer_queue; // protected by lock
	char canon_buf[512];        // protected by lock
	struct termios termios;     // protected by lock
	struct winsize size;        // protected by lock
	size_t column;              // protected by lock
	tty_ops_t *ops;
	size_t canon_index;         // protected by lock
	process_group_t *fg_group;  // protected by lock
	session_t *session;         // protected by lock
	size_t lines_count;         // protected by lock
	size_t lines[256];          // protected by lock
	spinlock_t lock;
} tty_t;

typedef struct pty pty_t;
typedef struct pty_slave pty_slave_t;

struct pty {
	ringbuffer_t output_buffer; // protected by lock
	sleep_queue_t writer_queue; // protected by lock
	sleep_queue_t reader_queue; // protected by lock
	pty_slave_t *slave; // cannot acquire if holding lock
	spinlock_t lock;
};

struct pty_slave {
	tty_t tty;
	pty_t *pty;
};

int tty_raw_add_input(tty_t *tty, const char *buffer, size_t count);
int tty_add_input(tty_t *tty, const char *buffer, size_t count);
int tty_do_ioctl(tty_t *fd, long request, void *arg);
int tty_register(tty_t *tty, const char *fmt, dev_t number);

static inline tty_t *tty_ref(tty_t *tty) {
	if (tty) device_ref(&tty->device);
	return tty;
}

static inline void tty_release(tty_t *tty) {
	if (tty) device_release(&tty->device);
}

int new_pty(vfs_fd_t **master, vfs_fd_t **slave, tty_t **);

void init_ptys(void);

#endif
