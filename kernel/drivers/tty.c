#include <kernel/vfs.h>
#include <kernel/tty.h>
#include <kernel/string.h>
#include <kernel/kheap.h>
#include <kernel/scheduler.h>

ssize_t tty_read(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	(void)offset;
	struct tty *tty = (struct tty *)node->private_inode;

	if(!(tty->termios.c_lflag & ICANON)){
		//wait until there enought data to read
		if(ringbuffer_read_available(&tty->input_buffer) < (size_t)tty->termios.c_cc[VMIN]){
			list_append(tty->waiter,get_current_proc());
			block_proc();
		}
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

vfs_node *new_tty(tty **tty){
	if(!(*tty)){
		(*tty) = kmalloc(sizeof(struct tty));
		memset((*tty),0,sizeof(struct tty));
	}

	(*tty)->input_buffer = new_ringbuffer(4096);

	//reset termios to default value
	memset(&(*tty)->termios,0,sizeof(struct termios));
	(*tty)->termios.c_cc[VEOF] = 0x04;
	(*tty)->termios.c_cc[VERASE] = '\b';
	(*tty)->termios.c_cc[VINTR] = 0x03;
	(*tty)->termios.c_cc[VQUIT] = 0x22;
	(*tty)->termios.c_cc[VSUSP] = 0x20;
	(*tty)->termios.c_cc[VMIN] = 1;
	(*tty)->termios.c_iflag = ICRNL | IMAXBEL;
	(*tty)->termios.c_oflag = OPOST | ONLCR | ONLRET;
	(*tty)->termios.c_lflag = ECHO;

	(*tty)->canon_buf = kmalloc(512);
	
	//create the vfs node
	vfs_node *node = kmalloc(sizeof(vfs_node));
	node->private_inode = tty;
	node->flags = VFS_DEV | VFS_CHAR | VFS_TTY;
	node->read  = tty_read;
	node->write = tty_write;

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