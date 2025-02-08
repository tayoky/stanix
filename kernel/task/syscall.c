#include "sys.h"
#include "scheduler.h"
#include "print.h"
#include "kernel.h"
#include "idt.h"
#include "print.h"
#include <errno.h>

extern void syscall_handler();

void init_syscall(void){
	kstatus("init syscall... ");
	set_idt_gate(kernel->idt,0x80,syscall_handler,0xEF);
	kok();
}

int sys_open(const char *path,int flags,mode_t mode){
	kdebugf("app try to open %s\n",path);

	//imposible conbinason of flags
	if(flags & O_WRONLY && flags & O_RDWR){
		return -EINVAL;
	}
	if((!flags & O_WRONLY) && (!flags & O_RDWR)){
		//can't use trunc with readonly
		if(flags & O_TRUNC){
			return -EINVAL;
		}
	}


	//first find a fd for it
	int fd = 0;
	while(is_valid_fd(fd)){
		fd++;
		if(fd >= MAX_FD){
			//to much fd open
			return -ENXIO;
		}
	}

	//translate to vfs flags
	uint64_t vfs_flags = 0;
	if(flags & O_WRONLY){
		vfs_flags = VFS_WRITEONLY;
	} else if (flags & O_RDWR){
		vfs_flags = VFS_READWRITE;
	} else {
		vfs_flags = VFS_READONLY;
	}
	

	file_descriptor *file = &get_current_proc()->fds[fd];
	vfs_node *node = vfs_open(path,vfs_flags);

	//O_CREAT things
	if(flags & O_CREAT){
		if(node && flags & O_EXCL){
			vfs_close(node);
			return -EEXIST;
		}
		
		if(!node){
			//the user want to create the file
			int result = vfs_create(path,mode,VFS_FILE);

			if(result){
				//vfs_create failed
				return result;
			}

			node = vfs_open(path,vfs_flags);
		}
	}

	if(!node){
		return -ENOENT;
	}

	//is a directory check
	if(flags & O_DIRECTORY){
		if(!node->flags & VFS_DIR){
			vfs_close(node);
			return -ENOTDIR;
		}
	}

	//simple check for writing on directory
	if((flags & O_WRONLY || flags & O_RDWR) && node->flags & VFS_DIR){
		vfs_close(node);
		return -EISDIR;
	}

	//now init the fd
	file->node = node;
	file->present = 1;
	file->offset = 0;

	//apply append and trunc if needed
	if(flags & O_TRUNC){
		if(!vfs_truncate(node,0)){
			node->size = 0;
		}
	}
	if(flags & O_APPEND){
		file->offset = node->size;
	}

	return fd;
}

int sys_close(int fd){
	if(!is_valid_fd(fd)){
		return -EBADF;
	}
	file_descriptor *file = &get_current_proc()->fds[fd];
	vfs_close(file->node);
	file->present = 0;
	return 0;
}

int64_t sys_write(int fd,void *buffer,size_t count){
	if(!is_valid_fd(fd)){
		return -EBADF;
	}
	file_descriptor *file = &get_current_proc()->fds[fd];
	int64_t wsize = vfs_write(file->node,buffer,file->offset,count);

	if(wsize > 0){
		file->offset += wsize;
	}

	return wsize;
}

int64_t sys_read(int fd,void *buffer,size_t count){
	if(!is_valid_fd(fd)){
		return -EBADF;
	}
	file_descriptor *file = &get_current_proc()->fds[fd];
	int64_t rsize = vfs_read(file->node,buffer,file->offset,count);

	if(rsize > 0){
		file->offset += rsize;
	}

	return rsize;
}

void sys_exit(uint64_t error_code){
	kdebugf("exit with code : %ld\n",error_code);
	kill_proc(get_current_proc());
}

pid_t sys_getpid(){
	return get_current_proc()->pid;
}