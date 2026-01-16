#include <kernel/tty.h>
#include <kernel/string.h>
#include <kernel/kheap.h>
#include <kernel/scheduler.h>
#include <kernel/print.h>
#include <kernel/kernel.h>
#include <kernel/signal.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <poll.h>

static ssize_t tty_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	(void)offset;
	tty_t *tty = (tty_t *)fd->private;

	if(tty->termios.c_lflag & ICANON){
		ssize_t rsize = ringbuffer_read(&tty->input_buffer, buffer, count, fd->flags);
		if(rsize < 0){
			return rsize;
		}
		if(((char *)buffer)[rsize - 1] == tty->termios.c_cc[VEOF]){
			rsize--;
		}
		return rsize;
	}

	return ringbuffer_read(&tty->input_buffer, buffer, count, fd->flags);
}

static ssize_t tty_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count){
	(void)offset;
	tty_t *tty = (tty_t *)fd->private;

	while(count > 0){
		tty_output(tty,*(char *)buffer);
		(char *)buffer++;
		count--;
	}
	return count;
}

static int tty_wait_check(vfs_fd_t *fd,short type){
	tty_t *tty = (tty_t *)fd->private;
	int events = 0;
	if ((type & POLLHUP) && tty->unconnected) {
		events |= POLLHUP;
	}

	if ((type & POLLIN) && ringbuffer_read_available(&tty->input_buffer)) {
		events |= POLLIN;
	}

	if (type & POLLOUT) {
		events |= POLLOUT;
	}

	return events;
}

static void tty_destroy(device_t *device){
	tty_t *tty = (tty_t *)device;

	// TODO : send SIGHUP

	if (tty->ops->cleanup) tty->ops->cleanup(tty);

	destroy_ringbuffer(&tty->input_buffer);
	kfree(tty->canon_buf);
}

static int tty_ioctl(vfs_fd_t *fd, long request, void *arg) {
	tty_t *tty = (tty_t *)fd->private;
	switch (request) {
	case TIOCGETA:
		*(struct termios *)arg = tty->termios;
		return 0;
	case TIOCSETA:
	case TIOCSETAF:
	case TIOCSETAW:
		tty->termios = *(struct termios *)arg;
		return 0;
	case TIOCGPGRP:
		*(pid_t *)arg =  tty->fg_pgrp;
		return 0;
	case TIOCSPGRP:
		//TODO : check if group exist
		kdebugf("set fgpgrp to %ld\n",*(pid_t *)arg);
		tty->fg_pgrp = *(pid_t *)arg;
		return 0;
	case TIOCSWINSZ:
		tty->size = *(struct winsize *)arg;
		return 0;
	case TIOCGWINSZ:
		*(struct winsize *)arg = tty->size;
		return 0;
	default:
		if (tty->ops->ioctl) {
			return tty->ops->ioctl(tty, request, arg);
		}
		return -EINVAL;
	}
}

static vfs_ops_t tty_ops = {
	.read       = tty_read,
	.write      = tty_write,
	.ioctl      = tty_ioctl,
	.wait_check = tty_wait_check,
};

tty_t *new_tty(tty_t *tty) {
	if(!tty){
		tty = kmalloc(sizeof(tty_t));
		memset(tty,0,sizeof(tty_t));
	}

	init_ringbuffer(&tty->input_buffer, 4096);

	//reset termios to default value
	memset(&tty->termios,0,sizeof(struct termios));
	tty->termios.c_cc[VEOF] = 0x04;
	tty->termios.c_cc[VERASE] = 127;
	tty->termios.c_cc[VINTR] = 0x03;
	tty->termios.c_cc[VQUIT] = 0x22;
	tty->termios.c_cc[VSUSP] = 0x1A;
	tty->termios.c_cc[VMIN] = 1;
	tty->termios.c_iflag = ICRNL | IMAXBEL;
	tty->termios.c_oflag = OPOST | ONLCR | ONLRET;
	tty->termios.c_lflag = ECHONL | ECHOK | ECHOE | ECHO | ICANON | IEXTEN | ISIG;
	tty->termios.c_oflag = CS8;

	tty->canon_buf = kmalloc(512);
	tty->canon_index = 0;
	tty->device.type = DEVICE_CHAR;
	tty->device.ops = &tty_ops;
	tty->device.destroy = tty_destroy;

	return tty;
}

// tty_output and tty_input based on TorauOS's tty system

int tty_output(tty_t *tty, char c) {
	if (tty->termios.c_oflag & OPOST) {
		//enable output processing
		if (tty->termios.c_oflag & OLCUC) {
			//map lowercase to uppercase
			if(c >= 'a' && c <= 'z') c += 'A' - 'a';
		}

		if (tty->termios.c_oflag & ONLCR) {
			//map NL to NL-CR
			if(c == '\n') tty_output(tty,'\r');
		}

		if (tty->termios.c_oflag & OCRNL) {
			//translate CR to NL
			if(c == '\r') c = '\n';
		}

		if (tty->termios.c_oflag & ONOCR) {
			//don't output CR at column 0
			if(c == '\r' && tty->column == 0) return 0;
		}
	}

	//CR (or NL and ONLRET flags) reset the column to 0
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
	if (tty->termios.c_iflag & ICRNL){
		// translate CR to NL
		if (c == '\r') c = '\n';
	}
	if (tty->termios.c_iflag & IUCLC){
		// map uppercase to lowercase
		if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
	}
	if (tty->termios.c_iflag & ISTRIP) {
		// strip off eighth bit
		c &= 0x7F;
	}

	//signal support here
	if (tty->termios.c_lflag & ISIG) {
		if (c == tty->termios.c_cc[VINTR]) {
			if (tty->fg_pgrp) {
				send_sig_pgrp(tty->fg_pgrp, SIGINT);
			}
		}
		if (c == tty->termios.c_cc[VQUIT]) {
			if (tty->fg_pgrp) {
				send_sig_pgrp(tty->fg_pgrp, SIGQUIT);
			}
		}
		if (c == tty->termios.c_cc[VSUSP]) {
			if (tty->fg_pgrp)  {
				send_sig_pgrp(tty->fg_pgrp, SIGTSTP);
			}
		}
	}

	// canonical mode here
	if (tty->termios.c_lflag & ICANON) {
		if (tty->termios.c_lflag & ECHO) {
			if (c == tty->termios.c_cc[VERASE] && tty->termios.c_lflag & ECHOE) {
				if (tty->canon_index > 0){
					if (tty->canon_buf[tty->canon_index -1] && tty->canon_buf[tty->canon_index -1] <= 31 && tty->canon_buf[tty->canon_index -1] != '\n'){
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

		//line editing stuff
		if ((tty->termios.c_lflag & IEXTEN)) {
			if (tty->termios.c_cc[VERASE] == c) {
				if(tty->canon_index > 0){
					tty->canon_index--;
				}
				return 0;
			}
			if(tty->termios.c_cc[VKILL] == c){
				tty->canon_index = 0;
				return 0;
			}
		}

		tty->canon_buf[tty->canon_index] = c;
		tty->canon_index++;
		if (c == '\n' || c == tty->termios.c_cc[VEOL] || c == tty->termios.c_cc[VEOF]) {
			if ((size_t)ringbuffer_write(&tty->input_buffer, tty->canon_buf, tty->canon_index, 0) < tty->canon_index) {
				if (tty->termios.c_iflag & IMAXBEL){
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

	//check for full ringbuffer
	if (ringbuffer_write(&tty->input_buffer, &c, 1, 0) == 0) {
		if (tty->termios.c_iflag & IMAXBEL) {
			tty_output(tty, '\a');
		}
	}

	return 0;
}
