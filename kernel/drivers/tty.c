#include <kernel/kernel.h>
#include <kernel/kheap.h>
#include <kernel/print.h>
#include <kernel/scheduler.h>
#include <kernel/userspace.h>
#include <kernel/signal.h>
#include <kernel/poll.h>
#include <kernel/string.h>
#include <kernel/tty.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <ctype.h>
#include <poll.h>

static int tty_output(tty_t *tty, char c) {
	spinlock_assert_acquired(&tty->lock);
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

static int tty_max_bell(tty_t *tty) {
	spinlock_assert_acquired(&tty->lock);
	if (tty->termios.c_iflag & IMAXBEL) {
		return tty_output(tty, '\a');
	}
	return 0;
}

static int tty_canon_send_line(tty_t *tty) {
	spinlock_assert_acquired(&tty->lock);
	ssize_t ret;
	if ((ret = ringbuffer_write(&tty->input_buffer, tty->canon_buf, tty->canon_index)) < (ssize_t)tty->canon_index) {
		tty_max_bell(tty);
	}

	if (tty->lines_count >= arraylen(tty->lines)) {
		tty_max_bell(tty);
		return 0;
	}

	tty->lines[tty->lines_count++] = ret;
	tty->canon_index = 0;
	wakeup_queue(&tty->reader_queue, 0);
	return 0;
}

static int tty_has_lflag(tty_t *tty, tcflag_t flags) {
	spinlock_assert_acquired(&tty->lock);
	return (tty->termios.c_lflag & flags) == flags;
}

static int tty_erase(tty_t *tty, char c) {
	spinlock_assert_acquired(&tty->lock);
	if (iscntrl(c) && tty_has_lflag(tty, ECHOCTL)) {
		// if char is a control char we need to earse both the char and the ^
		tty_output(tty, '\b');
		tty_output(tty, ' ');
		tty_output(tty, '\b');
	}
	tty_output(tty, '\b');
	tty_output(tty, ' ');
	tty_output(tty, '\b');
	return 0;
}

static int tty_echo(tty_t *tty, char c) {
	if (c == '\n' && tty_has_lflag(tty, ECHONL | ICANON)) {
		return tty_output(tty, c);
	} else if (tty_has_lflag(tty, ECHO)) {
		if (c == '\n' || c == '\t') {
			return tty_output(tty, c);
		} else if (c == tty->termios.c_cc[VERASE] && tty_has_lflag(tty, ECHOE | ICANON)) {
			return tty_erase(tty, tty->canon_buf[tty->canon_index - 1]);
		} else if (c == tty->termios.c_cc[VKILL] && tty_has_lflag(tty, ECHOK | ICANON)) {
			for (size_t i = tty->canon_index; i > 0; i--) {
				return tty_erase(tty, tty->canon_buf[i - 1]);
			}
		} else if (iscntrl(c) && tty_has_lflag(tty, ECHOCTL)) {
			tty_output(tty, '^');
			return tty_output(tty, c + 'A' - 1);
		} else {
			return tty_output(tty, c);
		}
	}
	return 0;
}

static int tty_input(tty_t *tty, char c) {
	spinlock_assert_acquired(&tty->lock);
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

	// signal support
	if (tty->termios.c_lflag & ISIG) {
		if (c == tty->termios.c_cc[VINTR]) {
			tty_echo(tty, c);
			signal_send_group(tty->fg_group, SIGINT);
			return 0;
		}
		if (c == tty->termios.c_cc[VQUIT]) {
			tty_echo(tty, c);
			signal_send_group(tty->fg_group, SIGQUIT);
			return 0;
		}
		if (c == tty->termios.c_cc[VSUSP]) {
			tty_echo(tty, c);
			signal_send_group(tty->fg_group, SIGTSTP);
			return 0;
		}
	}

	// canonical mode
	if (tty->termios.c_lflag & ICANON) {
		// line editing stuff
		if (tty->termios.c_cc[VERASE] == c) {
			if (tty->canon_index > 0) {
				tty_echo(tty, c);
				tty->canon_index--;
			}
			return 0;
		} else if (tty->termios.c_cc[VKILL] == c) {
			tty_echo(tty, c);
			tty->canon_index = 0;
			return 0;
		} else if (tty->termios.c_cc[VEOF] == c) {
			tty_canon_send_line(tty);
			return 0;
		} else {
			if (tty->canon_index >= sizeof(tty->canon_buf)) {
				tty_max_bell(tty);
			} else {
				tty->canon_buf[tty->canon_index++] = c;
				tty_echo(tty, c);
			}

			if (c == '\n' || c == tty->termios.c_cc[VEOL]) {
				tty_canon_send_line(tty);
			}
		}
		return 0;
	}

	tty_echo(tty, c);

	// check for full ringbuffer
	if (ringbuffer_write(&tty->input_buffer, &c, sizeof(c)) == 0) {
		tty_max_bell(tty);
		return 0;
	}
	wakeup_queue(&tty->reader_queue, 0);

	return 0;
}

int tty_raw_add_input(tty_t *tty, const char *buffer, size_t count) {
	spinlock_assert_acquired(&tty->lock);
	ssize_t total = 0;
	while (count > 0) {
		tty_input(tty, *buffer);
		count--;
		total++;
		buffer++;
	}
	return total;
}

int tty_add_input(tty_t *tty, const char *buffer, size_t count) {
	int irq_save = spinlock_acquire_irq(&tty->lock);
	ssize_t ret = tty_raw_add_input(tty, buffer, count);
	spinlock_release_irq(&tty->lock, irq_save);
	return ret;
}

static int tty_end_read_sleep(tty_t *tty) {
	if (device_is_unplugged(&tty->device)) {
		return 1;
	}
	 
	if (tty->termios.c_lflag & ICANON) {
		if (tty->lines_count > 0) {
			return 1;
		}
	} else {
		if (ringbuffer_read_available(&tty->input_buffer) >= (size_t)tty->termios.c_cc[VMIN]) {
			return 1;
		}
	}
	return 0;
}

// TODO : respect line bounds on canonical mode
static ssize_t tty_raw_read(tty_t *tty, char *buffer, size_t count, long flags) {
	// sleep until read available
	if (!(flags & O_NONBLOCK)) {
		if (sleep_on_queue_lock_interruptible(&tty->reader_queue, &tty->lock, tty_end_read_sleep(tty)) < 0) {
			return -EINTR;
		}
	}

	ssize_t ret = 0; 
	if (tty->termios.c_lflag & ICANON) {
		if (tty->lines_count <= 0) {
			if (device_is_unplugged(&tty->device)) {
				return 0;
			} else {
				return -EAGAIN;
			}
		}
		if (count > tty->lines[0]) count = tty->lines[0]; 
		ret = ringbuffer_read(&tty->input_buffer, buffer, count);
		if (ret >= 0) {
			tty->lines[0] -= ret;
			if (tty->lines[0] == 0) {
				tty->lines_count--;
				memmove(&tty->lines[0], &tty->lines[1], sizeof(size_t) * tty->lines_count);
			}
		}
	} else {
		if (ringbuffer_read_available(&tty->input_buffer) < (size_t)tty->termios.c_cc[VMIN]) {
			if (device_is_unplugged(&tty->device)) {
				return 0;
			} else {
				return -EAGAIN;
			}
		}
		ret = ringbuffer_read(&tty->input_buffer, buffer, count);
	}

	if (ret >= 0) {
		wakeup_queue(&tty->writer_queue, 0);
	}
	return ret;
}

static ssize_t tty_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	(void)offset;
	tty_t *tty = (tty_t *)fd->private;
	int irq_save = spinlock_acquire_irq(&tty->lock);
	ssize_t ret = tty_raw_read(tty, buffer, count, fd->flags);
	spinlock_release_irq(&tty->lock, irq_save);
	return ret;
}

static ssize_t tty_raw_write(tty_t *tty, const char *buffer, size_t count) {
	ssize_t total = 0;
	while (count > 0) {	
		char kbuf[128];
		size_t w = sizeof(kbuf) < count ? sizeof(kbuf) : count;
		int ret = safe_copy_from(kbuf, buffer, w);
		if (ret < 0) return total > 0 ? total : ret;

		for (size_t i=0; i<w; i++) {
			tty_output(tty, kbuf[i]);
		}
		count -= w;
		buffer += w;
		total += w;
	}
	return total;
}

static ssize_t tty_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	(void)offset;
	tty_t *tty = (tty_t *)fd->private;
	int irq_save = spinlock_acquire_irq(&tty->lock);
	ssize_t ret = tty_raw_write(tty, buffer, count);
	spinlock_release_irq(&tty->lock, irq_save);
	return ret;
}

static int tty_poll_add(vfs_fd_t *fd, poll_event_t *event) {
	(void)event;
	tty_t *tty = (tty_t *)fd->private;
	int irq_save = spinlock_acquire_irq(&tty->lock);
	// cannot wait on disconnected ttys
	if (!device_is_unplugged(&tty->device)) {
		sleep_add_to_queue(&tty->reader_queue);
	}
	spinlock_release_irq(&tty->lock, irq_save);
	return 0;
}

static int tty_poll_remove(vfs_fd_t *fd, poll_event_t *event) {
	(void)event;
	tty_t *tty = (tty_t *)fd->private;
	int irq_save = spinlock_acquire_irq(&tty->lock);
	sleep_remove_from_queue(&tty->reader_queue);
	spinlock_release_irq(&tty->lock, irq_save);
	return 0;
}

static int tty_poll_get(vfs_fd_t *fd, poll_event_t *event) {
	tty_t *tty = (tty_t *)fd->private;
	int irq_save = spinlock_acquire_irq(&tty->lock);
	if (device_is_unplugged(&tty->device)) event->revents |= POLLHUP;
	if (tty->termios.c_lflag & ICANON) {
		if (tty->lines_count > 0) event->revents |= POLLIN;
	} else {
		if (ringbuffer_read_available(&tty->input_buffer) >= (size_t)tty->termios.c_cc[VMIN]) event->revents |= POLLIN;
	}

	// technically we sometimes cannot write
	// but who care ?
	event->revents |= POLLOUT;

	spinlock_release_irq(&tty->lock, irq_save);
	return 0;
}

static void tty_destroy(device_t *device) {
	tty_t *tty = (tty_t *)device;

	int irq_save = spinlock_acquire_irq(&tty->lock);

	// TODO : send SIGHUP
	
	process_group_release(tty->fg_group);
	wakeup_queue(&tty->reader_queue, 0);
	wakeup_queue(&tty->writer_queue, 0);
	spinlock_release_irq(&tty->lock, irq_save);

	if (tty->ops->cleanup) tty->ops->cleanup(tty);

	ringbuffer_destroy(&tty->input_buffer);
}

static int tty_termios_update(tty_t *tty, struct termios *new) {
	if (tty->ops->update_termios) {
		int ret = tty->ops->update_termios(tty, new);
		if (ret < 0) return ret;
	}
	if ((new->c_lflag & ICANON) && !(tty->termios.c_lflag & ICANON)) {
		// entering canonical mode
		// throw the whole buffer as a line
		if (ringbuffer_read_available(&tty->input_buffer) > 0) {
			tty->lines[0] = ringbuffer_read_available(&tty->input_buffer);
			tty->lines_count = 1;
		} else {
			tty->lines_count = 0;
		}
		tty->canon_index = 0;
	}
	if (!(new->c_lflag & ICANON) && (tty->termios.c_lflag & ICANON)) {
		// exiting canonical mode
		// send the on going line
		if (tty->canon_index > 0) {
			tty_canon_send_line(tty);
		}
	}

	tty->termios = *new;
	
	// modifing termios can change wakeup conditions
	wakeup_queue(&tty->reader_queue, 0);

	return 0;
}

static int tty_do_raw_ioctl(tty_t *tty, long request, void *arg) {
	switch (request) {
	case TIOCGETA:
		return safe_copy_auto_to(arg, &tty->termios);
	case TIOCSETA:
	case TIOCSETAF:
	case TIOCSETAW:;
		struct termios termios;
		safe_copy_auto_from(&termios, arg);
		return tty_termios_update(tty, &termios);
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
	int irq_save = spinlock_acquire_irq(&tty->lock);
	int ret = tty_do_raw_ioctl(tty, request, arg);
	spinlock_release_irq(&tty->lock, irq_save);
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

int tty_register(tty_t *tty, const char *fmt, dev_t number) {
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
	tty->termios.c_lflag      = ECHONL | ECHOK | ECHOE | ECHOCTL | ECHO | ICANON | IEXTEN | ISIG;
	tty->termios.c_cflag      = CS8;

	tty->canon_index    = 0;
	tty->device.type    = DEVICE_CHAR;
	tty->device.ops     = &tty_ops;
	tty->device.destroy = tty_destroy;

	return device_register(&tty->device, fmt, number);
}
