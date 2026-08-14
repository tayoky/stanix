#include <kernel/kernel.h>
#include <kernel/kheap.h>
#include <kernel/print.h>
#include <kernel/scheduler.h>
#include <kernel/userspace.h>
#include <kernel/signal.h>
#include <kernel/string.h>
#include <kernel/tty.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <poll.h>

static ssize_t tty_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	(void)offset;
	tty_t *tty = (tty_t *)fd->private;

	if (tty->termios.c_lflag & ICANON) {
		ssize_t rsize = ringbuffer_read(&tty->input_buffer, buffer, count, fd->flags);
		if (rsize < 0) {
			return rsize;
		}
		if (((char *)buffer)[rsize - 1] == tty->termios.c_cc[VEOF]) {
			rsize--;
		}
		return rsize;
	}

	return ringbuffer_read(&tty->input_buffer, buffer, count, fd->flags);
}

static ssize_t tty_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	(void)offset;
	tty_t *tty = (tty_t *)fd->private;

	const char *buf = buffer;
	size_t remaining = count;
	while (remaining > 0) {	
		char kbuf[128];
		size_t w = sizeof(kbuf) < remaining ? sizeof(kbuf) : remaining;
		int ret = safe_copy_from(kbuf, buf, w);
		if (ret < 0) return ret;

		for (size_t i=0; i<w; i++) {
			tty_output(tty, kbuf[i]);
		}
		remaining -= w;
		buf += w;
	}
	return count;
}

static int tty_poll_add(vfs_fd_t *fd, poll_event_t *event) {
	tty_t *tty = (tty_t *)fd->private;

	if (device_is_unplugged(&tty->device)) {
		// cannot wait on disconnected ttys
		return 0;
	}

	if (event->events & (POLLIN | POLLHUP)) {
		sleep_add_to_queue(&tty->input_buffer.reader_queue);
	}

	return 0;
}

static int tty_poll_remove(vfs_fd_t *fd, poll_event_t *event) {
	tty_t *tty = (tty_t *)fd->private;
	if (event->events & (POLLIN | POLLHUP)) {
		sleep_remove_from_queue(&tty->input_buffer.reader_queue);
	}
	return 0;
}

static int tty_poll_get(vfs_fd_t *fd, poll_event_t *event) {
	tty_t *tty = (tty_t *)fd->private;

	if (device_is_unplugged(&tty->device)) {
		event->revents |= POLLHUP;
	}

	if (ringbuffer_read_available(&tty->input_buffer)) event->revents |= POLLIN;

	// technicly we sometimes cannot write
	// but who care ?
	event->revents |= POLLOUT;

	return 0;
}

static void tty_destroy(device_t *device) {
	tty_t *tty = (tty_t *)device;

	spinlock_acquire(&tty->lock);

	// TODO : send SIGHUP
	
	process_group_release(tty->fg_group);
	spinlock_release(&tty->lock);

	if (tty->ops->cleanup) tty->ops->cleanup(tty);

	ringbuffer_wakeup_all(&tty->input_buffer);
	ringbuffer_destroy(&tty->input_buffer);
	kfree(tty->canon_buf);
}

static int tty_do_raw_ioctl(tty_t *tty, long request, void *arg) {
	switch (request) {
	case TIOCGETA:
		return safe_copy_auto_to(arg, &tty->termios);
	case TIOCSETA:
	case TIOCSETAF:
	case TIOCSETAW:
		return safe_copy_auto_from(&tty->termios, arg);
	case TIOCGPGRP:;
		pid_t pgid = tty->fg_group ? tty->fg_group->pgid : 1;
		return safe_copy_auto_to(arg, &pgid);
	case TIOCSPGRP:
		pgid = 0;
		if (safe_copy_auto_from(&pgid, arg) < 0) return -EFAULT;
		process_group_t *group = process_group_from_pgid(pgid);
		if (!group) return -ESRCH;
		kdebugf("set fgpgrp to %ld\n", group->pgid);
		process_group_release(tty->fg_group);
		tty->fg_group = group;
		return 0;
	case TIOCSWINSZ:
		if (safe_copy_auto_from(&tty->size, arg) < 0) return -EFAULT;
		signal_send_group(tty->fg_group, SIGWINCH);
		return 0;
	case TIOCGWINSZ:
		return safe_copy_auto_to(arg, &tty->size);
	default:
		if (tty->ops->ioctl) {
			return tty->ops->ioctl(tty, request, arg);
		}
		return -EINVAL;
	}
}

int tty_do_ioctl(tty_t *tty, long request, void *arg) {
	spinlock_acquire(&tty->lock);
	int ret = tty_do_raw_ioctl(tty, request, arg);
	spinlock_release(&tty->lock);
	return ret;
}

static int tty_ioctl(vfs_fd_t *fd, long request, void *arg) {
	tty_t *tty = (tty_t *)fd->private;
	return tty_do_ioctl(tty, request, arg);
}

static vfs_fd_ops_t tty_ops = {
	.read        = tty_read,
	.write       = tty_write,
	.ioctl       = tty_ioctl,
	.poll_add    = tty_poll_add,
	.poll_remove = tty_poll_remove,
	.poll_get    = tty_poll_get,
};

int tty_register(tty_t *tty, const char *fmt, dev_t number);
	ringbuffer_init(&tty->input_buffer, 4096);

	// reset termios to default value
	memset(&tty->termios, 0, sizeof(struct termios));
	tty->termios.c_cc[VEOF]   = 004;
	tty->termios.c_cc[VEOL]   = 000;
	tty->termios.c_cc[VERASE] = 0177;
	tty->termios.c_cc[VINTR]  = 003;
	tty->termios.c_cc[VKILL]  = 025;
	tty->termios.c_cc[VQUIT]  = 034;
	tty->termios.c_cc[VSUSP]  = 032;
	tty->termios.c_cc[VMIN]   = 1;
	tty->termios.c_iflag      = ICRNL | IMAXBEL;
	tty->termios.c_oflag      = OPOST | ONLCR;
	tty->termios.c_lflag      = ECHONL | ECHOK | ECHOE | ECHO | ICANON | IEXTEN | ISIG;
	tty->termios.c_cflag      = CS8;

	tty->canon_buf      = kmalloc(512);
	tty->canon_index    = 0;
	tty->device.type    = DEVICE_CHAR;
	tty->device.ops     = &tty_ops;
	tty->device.destroy = tty_destroy;

	return device_register(&tty->device, fmt, number);
}

// tty_output and tty_input based on TorauOS's tty system

int tty_output(tty_t *tty, char c) {
	if (tty->termios.c_oflag & OPOST) {
		// enable output processing
		if (tty->termios.c_oflag & OLCUC) {
			// map lowercase to uppercase
			if (c >= 'a' && c <= 'z') c += 'A' - 'a';
		}

		if (tty->termios.c_oflag & ONLCR) {
			// map NL to NL-CR
			if (c == '\n') tty_output(tty, '\r');
		}

		if (tty->termios.c_oflag & OCRNL) {
			// translate CR to NL
			if (c == '\r') c = '\n';
		}

		if (tty->termios.c_oflag & ONOCR) {
			// don't output CR at column 0
			if (c == '\r' && tty->column == 0) return 0;
		}
	}

	// CR (or NL and ONLRET flags) reset the column to 0
	if (c == '\r' || (c == '\n' && tty->termios.c_oflag & ONLRET)) {
		tty->column = 0;
	} else {
		tty->column++;
	}
	tty->ops->out(tty, &c, 1);
	return 0;
}

int tty_input(tty_t *tty, char c) {
	if (tty->termios.c_iflag & INLCR) {
		// translate NL to CR
		if (c == '\n') c = '\r';
	}
	if (tty->termios.c_iflag & IGNCR) {
		// ignore CR
		if (c == '\r') return 0;
	}
	if (tty->termios.c_iflag & ICRNL) {
		// translate CR to NL
		if (c == '\r') c = '\n';
	}
	if (tty->termios.c_iflag & IUCLC) {
		// map uppercase to lowercase
		if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
	}
	if (tty->termios.c_iflag & ISTRIP) {
		// strip off eighth bit
		c &= 0x7F;
	}

	// signal support here
	if (tty->termios.c_lflag & ISIG) {
		if (c == tty->termios.c_cc[VINTR]) {
			signal_send_group(tty->fg_group, SIGINT);
		}
		if (c == tty->termios.c_cc[VQUIT]) {
			signal_send_group(tty->fg_group, SIGQUIT);
		}
		if (c == tty->termios.c_cc[VSUSP]) {
			signal_send_group(tty->fg_group, SIGTSTP);
		}
	}

	// canonical mode here
	if (tty->termios.c_lflag & ICANON) {
		if (tty->termios.c_lflag & ECHO) {
			if (c == tty->termios.c_cc[VERASE] && tty->termios.c_lflag & ECHOE) {
				if (tty->canon_index > 0) {
					if (tty->canon_buf[tty->canon_index - 1] && tty->canon_buf[tty->canon_index - 1] <= 31 && tty->canon_buf[tty->canon_index - 1] != '\n') {
						// if last is a control char we need to earse both the char and the ^
						tty_output(tty, '\b');
						tty_output(tty, ' ');
						tty_output(tty, '\b');
					}
					tty_output(tty, '\b');
					tty_output(tty, ' ');
					tty_output(tty, '\b');
				}
			} else if (c && c <= 31 && c != '\n') {
				tty_output(tty, '^');
				tty_output(tty, c + 'A' - 1);
			} else {
				tty_output(tty, c);
			}
		} else if (c == '\n' && (tty->termios.c_lflag & ECHONL)) {
			tty_output(tty, '\n');
		}

		// line editing stuff
		if ((tty->termios.c_lflag & IEXTEN)) {
			if (tty->termios.c_cc[VERASE] == c) {
				if (tty->canon_index > 0) {
					tty->canon_index--;
				}
				return 0;
			}
			if (tty->termios.c_cc[VKILL] == c) {
				tty->canon_index = 0;
				return 0;
			}
		}

		tty->canon_buf[tty->canon_index] = c;
		tty->canon_index++;
		if (c == '\n' || c == tty->termios.c_cc[VEOL] || c == tty->termios.c_cc[VEOF]) {
			if ((size_t)ringbuffer_write(&tty->input_buffer, tty->canon_buf, tty->canon_index, 0) < tty->canon_index) {
				if (tty->termios.c_iflag & IMAXBEL) {
					tty_output(tty, '\a');
				}
			}
			tty->canon_index = 0;
		}
		return 0;
	}

	if (tty->termios.c_lflag & ECHO) {
		tty_output(tty, c);
	}

	// check for full ringbuffer
	if (ringbuffer_write(&tty->input_buffer, &c, 1, 0) == 0) {
		if (tty->termios.c_iflag & IMAXBEL) {
			tty_output(tty, '\a');
		}
	}

	return 0;
}
