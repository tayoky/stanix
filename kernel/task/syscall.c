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

static int find_fd(){
	int fd = 0;
	while(is_valid_fd(fd)){
		fd++;
	}

	if(fd >= MAX_FD){
		//to much fd open
		return -1;
	}

	return fd;
}

int sys_open(const char *path,int flags,mode_t mode){
	kdebugf("app try to open %s\n",path);

	//imposible conbinason of flags
	if(flags & O_WRONLY && flags & O_RDWR){
		return -EINVAL;
	}
	if((!flags & O_WRONLY) && (!(flags & O_RDWR))){
		//can't use trunc with readonly
		if(flags & O_TRUNC){
			return -EINVAL;
		}
	}


	//first find a fd for it
	int fd = find_fd();
	if(fd == -1){
		return -ENXIO;
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
		if(!(node->flags & VFS_DIR)){
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

	//now apply the flags on the fd
	file->flags = 0;
	if(flags & O_WRONLY){
		file->flags |= FD_WRITE;
	} else if (flags & O_RDWR){
		file->flags |= FD_WRITE | FD_READ;
	} else {
		file->flags |= FD_READ;
	}

	if(flags & O_APPEND){
		file->flags |= FD_APPEND;
	}

	if(flags & O_CLOEXEC){
		file->flags |= FD_CLOEXEC;
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
	if((!is_valid_fd(fd) || (!FD_CHECK(fd,FD_WRITE)))){
		return -EBADF;
	}


	file_descriptor *file = &get_current_proc()->fds[fd];

	//if append go to the end
	if(FD_CHECK(fd,FD_APPEND)){
		file->offset = file->node->size;
	}

	int64_t wsize = vfs_write(file->node,buffer,file->offset,count);

	if(wsize > 0){
		file->offset += wsize;
	}

	return wsize;
}

int64_t sys_read(int fd,void *buffer,size_t count){
	if((!is_valid_fd(fd) || (!FD_CHECK(fd,FD_READ)))){
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

int sys_dup(int oldfd){
	//first find a newfd for it
	int newfd = find_fd();
	if(newfd == -1){
		return -ENXIO;
	}
	
	//use sys_dup2 to make the copy
	return sys_dup2(oldfd,newfd);
}

int sys_dup2(int oldfd, int newfd){
	//some checks
	if(oldfd == newfd){
		return newfd;
	}
	if(newfd < 0 || newfd >= MAX_FD){
		return -EBADF;
	}
	if(is_valid_fd(oldfd)){
		return -EBADF;
	}

	file_descriptor *old_file = &get_current_proc()->fds[oldfd];
	file_descriptor *new_file = &get_current_proc()->fds[newfd];

	//okay now we can close if aready open
	if(new_file->present){
		sys_close(newfd);
	}

	//make the actual copy
	new_file->node = vfs_dup(old_file->node);
	if(new_file->node){
		new_file->present = 1;
	} else {
		return -EIO;
	}
	return newfd;
}

int64_t sys_seek(int fd,int64_t offset,int whence){
	if(whence > 2){
		return -EINVAL;
	}

	if(!is_valid_fd(fd)){
		return -EBADF;
	}

	//get the fd
	file_descriptor *file = &get_current_proc()->fds[fd];

	switch (whence)
	{
	case SEEK_SET:
		file->offset = offset;
		break;
	case SEEK_CUR:
		file->offset += offset;
		break;
	case SEEK_END:
		file->offset = file->node->size + offset;
		break;
	default:
		break;
	}

	return file->offset;
}

pid_t sys_getpid(){
	return get_current_proc()->pid;
}

void *syscall_table[] = {
	(void *)sys_exit,
	(void *)sys_open,
	(void *)sys_read,
	(void *)sys_write,
	(void *)sys_seek,
	(void *)sys_dup,
	(void *)sys_dup2,
	(void *)sys_getpid,
};

uint64_t syscall_number = sizeof(syscall_table) / sizeof(void *);