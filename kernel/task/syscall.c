#include "sys.h"
#include "scheduler.h"
#include "print.h"
#include "kernel.h"
#include "idt.h"
#include "print.h"

extern void syscall_handler();

void init_syscall(void){
	kstatus("init syscall... ");
	set_idt_gate(kernel->idt,0x80,syscall_handler,0xEF);
	kok();
}

int sys_open(const char *path,int flags,mode_t mode){
	kdebugf("app try to open %s\n",path);
	//first find a fd for it
	int fd = 0;
	while(is_vaild_fd(fd)){
		fd++;
		if(fd >= MAX_FD){
			return -1;
		}
	}

	file_descriptor *file = &get_current_proc()->fds[fd];
	vfs_node *node = vfs_open(path);
	if(!node){
		return -1;
	}

	//now init the fd
	file->node = node;
	file->present = 1;
	file->offset = 0;
	return fd;
}

int sys_close(int fd){
	if(!is_vaild_fd(fd)){
		return -1;
	}
	file_descriptor *file = &get_current_proc()->fds[fd];
	vfs_close(file->node);
	file->present = 0;
	return 0;
}

int64_t sys_write(int fd,void *buffer,size_t count){
	if(!is_vaild_fd(fd)){
		return -1;
	}
	file_descriptor *file = &get_current_proc()->fds[fd];
	int64_t wsize = vfs_write(file->node,buffer,file->offset,count);

	if(wsize > 0){
		file->offset += wsize;
	}

	return wsize;
}

int64_t sys_read(int fd,void *buffer,size_t count){
	if(!is_vaild_fd(fd)){
		return -1;
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