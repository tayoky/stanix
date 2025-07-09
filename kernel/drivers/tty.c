#include <kernel/vfs.h>
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

void pty_output(char c,void *data){
	pty *pty = (struct pty*)data;
	ringbuffer_write(&c,&pty->output_buffer,1);
}

ssize_t pty_master_read(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	(void)offset;

	tty *tty = (struct tty *)node->private_inode;
	pty *pty = (struct pty *)tty->private_data;

	if(pty->slave->ref_count == 1 && !ringbuffer_read_available(&pty->output_buffer)){
		//nobody as open the slave and there no data
		return -EIO;
	}

	return ringbuffer_read(buffer,&pty->output_buffer,count);
}

ssize_t pty_master_write(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	(void)offset;

	tty *tty = (struct tty *)node->private_inode;
	for(size_t i=0;i<count;i++){
		tty_input(tty,*(char *)buffer);
		(char *)buffer++;
	}
	return (ssize_t)count;
}

int pty_master_wait_check(vfs_node *node,short type){
	tty *tty = (struct tty *)node->private_inode;
	pty *pty = (struct pty *)tty->private_data;
	int events = 0;
	if((type & POLLHUP) && pty->slave->ref_count == 1){
		events |= POLLHUP;
	}
	if((type & POLLIN) && ringbuffer_read_available(&pty->output_buffer)){
		events |= POLLIN;
	}
	if(type & POLLOUT){
		//we can alaway write to master pty
		events |= POLLOUT;
	}

	return events;
}

int new_pty(vfs_node **master,vfs_node **slave,tty **rep){
	pty *pty = kmalloc(sizeof(struct pty));
	memset(pty,0,sizeof(struct pty));
	pty->output_buffer = new_ringbuffer(4096);

	tty *tty = NULL;
	*slave = new_tty(&tty);
	*rep = tty;
	tty->private_data = pty;
	tty->out = pty_output;

	//create the master
	*master = kmalloc(sizeof(vfs_node));
	memset(*master,0,sizeof(vfs_node));
	(*master)->read          = pty_master_read;
	(*master)->write         = pty_master_write;
	(*master)->wait_check    = pty_master_wait_check;
	(*master)->private_inode = tty;
	(*master)->ref_count     = 1;

	//mount and save the slave
	pty->slave = *slave;
	char path[32];
	sprintf(path,"/dev/pts/%d",kernel->pty_count);
	if(vfs_mount(path,*slave)){
		vfs_close(*master);
		vfs_close(*slave);
		//TODO : delete tty
		return -ENOENT;
	}
	//mounting reset refcount to 1
	(*slave)->ref_count++;

	return kernel->pty_count++;
}

ssize_t tty_read(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	(void)offset;
	struct tty *tty = (struct tty *)node->private_inode;

	if(tty->termios.c_lflag & ICANON){
		ssize_t rsize = ringbuffer_read(buffer,&tty->input_buffer,count);
		if(rsize < 0){
			return rsize;
		}
		if(((char *)buffer)[rsize - 1] == tty->termios.c_cc[VEOF]){
			rsize--;
		}
		return rsize;
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

int tty_wait_check(vfs_node *node,short type){
	struct tty *tty = (struct tty *)node->private_inode;
	int events = 0;
	if((type & POLLHUP) && tty->unconnected){
		events |= POLLHUP;
	}

	if((type & POLLIN) && ringbuffer_read_available(&tty->input_buffer)){
		events |= POLLIN;
	}

	if(type & POLLOUT){
		events |= POLLOUT;
	}

	return events;
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
	(*tty)->termios.c_cc[VSUSP] = 0x1A;
	(*tty)->termios.c_cc[VMIN] = 1;
	(*tty)->termios.c_iflag = ICRNL | IMAXBEL;
	(*tty)->termios.c_oflag = OPOST | ONLCR | ONLRET;
	(*tty)->termios.c_lflag = ECHONL | ECHOK | ECHOE | ECHO | ICANON | IEXTEN | ISIG;

	(*tty)->canon_buf = kmalloc(512);
	(*tty)->canon_index = 0;
	
	//create the vfs node
	vfs_node *node = kmalloc(sizeof(vfs_node));
	memset(node,0,sizeof(vfs_node));
	node->private_inode = *tty;
	node->flags         = VFS_DEV | VFS_CHAR | VFS_TTY;
	node->read          = tty_read;
	node->write         = tty_write;
	node->ioctl         = tty_ioctl;
	node->wait_check    = tty_wait_check;

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

	//signal support here
	if(tty->termios.c_lflag & ISIG){
		if(c == tty->termios.c_cc[VINTR]){
			if(tty->fg_pgrp){
				send_sig_pgrp(tty->fg_pgrp,SIGINT);
			}
			return 0;
		}
		if(c == tty->termios.c_cc[VQUIT]){
			if(tty->fg_pgrp){
				send_sig_pgrp(tty->fg_pgrp,SIGQUIT);
			}
			return 0;
		}
		if(c == tty->termios.c_cc[VSUSP]){
			if(tty->fg_pgrp){
				send_sig_pgrp(tty->fg_pgrp,SIGTSTP);
			}
			return 0;
		}
	}

	//canonical mode here
	if(tty->termios.c_lflag & ICANON){
		if(tty->termios.c_lflag & ECHO){
			if(c == tty->termios.c_cc[VERASE] && tty->termios.c_lflag & ECHOE){
				if(tty->canon_index > 0){
					if(tty->canon_buf[tty->canon_index -1] && tty->canon_buf[tty->canon_index -1] <= 31 && tty->canon_buf[tty->canon_index -1] != '\n'){
						//if last is a control char we need to earse both the char and the ^
						tty_output(tty,'\b');
						tty_output(tty,' ');
						tty_output(tty,'\b');
					}
					tty_output(tty,'\b');
					tty_output(tty,' ');
					tty_output(tty,'\b');
				}
			} else if(c && c <= 31 && c != '\n'){
				tty_output(tty,'^');
				tty_output(tty,c + 'A' - 1);
			} else {
				tty_output(tty,c);
			}
		} else if(c == '\n' && (tty->termios.c_lflag & ECHONL)){
				tty_output(tty,'\n');
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
			if((size_t)ringbuffer_write(tty->canon_buf,&tty->input_buffer,tty->canon_index) < tty->canon_index){
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