#include <kernel/vfs.h>
#include <kernel/tty.h>
#include <kernel/string.h>
#include <kernel/kheap.h>
#include <kernel/scheduler.h>
#include <kernel/print.h>
#include <sys/ioctl.h>
#include <errno.h>

ssize_t tty_read(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	(void)offset;
	struct tty *tty = (struct tty *)node->private_inode;

	if(tty->termios.c_lflag & ICANON){
		size_t rsize = ringbuffer_read(buffer,&tty->input_buffer,count);
		if(((char *)buffer)[rsize - 1] == tty->termios.c_cc[VEOF]){
			rsize--;
		}
		return (ssize_t)rsize;
	}

	return ringbuffer_read(buffer,&tty->input_buffer,count);
}

ssize_t tty_write(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	(void)offset;
	struct tty *tty = (struct tty *)node->private_inode;

	while(count > 0){
		tty_output(tty,*(char *)buffer);
		(char *)buffer++;
		count--;
	}
	return count;
}

int tty_ioctl(vfs_node *node,uint64_t request,void *arg){
	struct tty *tty = (struct tty *)node->private_inode;
	switch (request){
	case TIOCGETA:
		*(struct termios *)arg = tty->termios;
		return 0;
	case TIOCSETA:
	case TIOCSETAF:
	case TIOCSETAW:
		tty->termios = *(struct termios *)arg;
		return 0;
	case TIOCGPGRP:
		if(tty->fg_proc){
			return -EINVAL;
		}
		return tty->fg_proc->pid;
	case TIOCSPGRP:
		process *proc = pid2proc((pid_t)(uintptr_t)arg);
		if(proc){
			tty->fg_proc = proc;
			return 0;
		}
		return -ESRCH;
	case TIOCSWINSZ:
		tty->size = *(struct winsize *)arg;
		return 0;
	case TIOCGWINSZ:
		*(struct winsize *)arg = tty->size;
		return 0;
	default:
		return -EINVAL;
		break;
	}
}

vfs_node *new_tty(tty **tty){
	if(!(*tty)){
		(*tty) = kmalloc(sizeof(struct tty));
		memset((*tty),0,sizeof(struct tty));
	}

	(*tty)->input_buffer = new_ringbuffer(4096);

	//reset termios to default value
	memset(&(*tty)->termios,0,sizeof(struct termios));
	(*tty)->termios.c_cc[VEOF] = 0x04;
	(*tty)->termios.c_cc[VERASE] = 127;
	(*tty)->termios.c_cc[VINTR] = 0x03;
	(*tty)->termios.c_cc[VQUIT] = 0x22;
	(*tty)->termios.c_cc[VSUSP] = 0x20;
	(*tty)->termios.c_cc[VMIN] = 1;
	(*tty)->termios.c_iflag = ICRNL | IMAXBEL;
	(*tty)->termios.c_oflag = OPOST | ONLCR | ONLRET;
	(*tty)->termios.c_lflag = ECHONL | ECHOK | ECHOE | ECHO | ICANON | IEXTEN;

	(*tty)->canon_buf = kmalloc(512);
	(*tty)->canon_index = 0;
	
	//create the vfs node
	vfs_node *node = kmalloc(sizeof(vfs_node));
	memset(node,0,sizeof(vfs_node));
	node->private_inode = *tty;
	node->flags = VFS_DEV | VFS_CHAR | VFS_TTY;
	node->read  = tty_read;
	node->write = tty_write;
	node->ioctl = tty_ioctl;

	return node;
}

int tty_output(tty *tty,char c){
	if(tty->termios.c_oflag & OPOST){
		//enable output processing
		if(tty->termios.c_oflag & OLCUC){
			//map lowercase to uppercase
			if(c >= 'a' && c <= 'z') c += 'A' - 'a';
		}

		if(tty->termios.c_oflag & ONLCR){
			//map NL to NL-CR
			if(c == '\n') tty_output(tty,'\r');
		}

		if(tty->termios.c_oflag & OCRNL){
			//translate CR to NL
			if(c == '\r') c = '\n';
		}

		if(tty->termios.c_oflag & ONOCR){
			//don't output CR at column 0
			if(c == '\r' && tty->column == 0) return 0;
		}
	}

	//CR (or NL and ONLRET flags) reset the column to 0
	if(c == '\r' || (c == '\n' && tty->termios.c_oflag & ONLRET)){
		tty->column = 0;
	} else {
		tty->column++;
	}
	tty->out(c,tty->private_data);
	return 0;
}

int tty_input(tty *tty,char c){
	if(tty->termios.c_iflag & INLCR){
		//translate NL to CR
		if(c == '\n') c = '\r';
	}
	if(tty->termios.c_iflag & IGNCR){
		//ignore CR
		if(c == '\r') return 0;
	}
	if(tty->termios.c_iflag & ICRNL){
		//translate CR to NL
		if(c == '\r') c = '\n';
	}
	if(tty->termios.c_iflag & IUCLC){
		//map uppercase to lowercase
		if(c >= 'A' && c <= 'Z') c += 'a' - 'A';
	}
	if(tty->termios.c_iflag & ISTRIP){
		//strip off eighth bit
		c &= 0x7F;
	}

	//canonical mode here
	if(tty->termios.c_lflag & ICANON){
		if(c == tty->termios.c_cc[VERASE] && tty->termios.c_lflag & ECHOE){
			if(tty->canon_index > 0){
				tty_output(tty,'\b');
				tty_output(tty,' ');
				tty_output(tty,'\b');
			}
		} else if(c == '\n' && (tty->termios.c_lflag & ECHONL)){
			tty_output(tty,'\n');
		} else if(tty->termios.c_lflag & ECHO){
			tty_output(tty,c);
		}

		//line editing stuff
		if((tty->termios.c_lflag & IEXTEN)){
			if(tty->termios.c_cc[VERASE] == c){
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
		if(c == '\n' || c == tty->termios.c_cc[VEOL] || c == tty->termios.c_cc[VEOF]){
			if(ringbuffer_write(tty->canon_buf,&tty->input_buffer,tty->canon_index) < tty->canon_index){
				if(tty->termios.c_iflag & IMAXBEL){
					tty_output(tty,'\a');
				}
			}
			tty->canon_index = 0;
		}
		return 0;
	}

	if(tty->termios.c_lflag & ECHO){
		tty_output(tty,c);
	}

	//check for full ringbuffer
	if(ringbuffer_write(&c,&tty->input_buffer,1) == 0){
		if(tty->termios.c_iflag & IMAXBEL){
			tty_output(tty,'\a');
		}
	}

	return 0;
}