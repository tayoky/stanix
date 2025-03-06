#include "sys.h"
#include "scheduler.h"
#include "print.h"
#include "kernel.h"
#include "idt.h"
#include "print.h"
#include <errno.h>
#include "page.h"
#include "paging.h"
#include "sleep.h"
#include <sys/type.h>
#include <dirent.h>
#include <sys/stat.h>
#include "pipe.h"
#include "exec.h"
#include "memseg.h"
#include "fork.h"

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
	

	file_descriptor *file = &FD_GET(fd);
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
	} else {
		//the user don't want a dir but got one
		//not good
		if(node->flags & VFS_DIR){
			vfs_close(node);
			return -EISDIR;
		}
	}
	

	//simple check for writing on directory
	if((flags & O_WRONLY || flags & O_RDWR || flags & O_TRUNC) && node->flags & VFS_DIR){
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
	kdebugf("try close fd %ld\n",fd);
	if(!is_valid_fd(fd)){
		return -EBADF;
	}
	file_descriptor *file = &FD_GET(fd);
	vfs_close(file->node);
	file->present = 0;
	return 0;
}

int64_t sys_write(int fd,void *buffer,size_t count){
	if((!is_valid_fd(fd) || (!FD_CHECK(fd,FD_WRITE)))){
		return -EBADF;
	}


	file_descriptor *file = &FD_GET(fd);

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
	file_descriptor *file = &FD_GET(fd);
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
	if(!is_valid_fd(oldfd)){
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
	file_descriptor *file = &FD_GET(fd);

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

uint64_t sys_sbrk(intptr_t incr){
	//get rid of the 0 case
	if(!incr){
		return get_current_proc()->heap_end;
	}

	//get proc
	process *proc = get_current_proc();

	int64_t incr_pages = PAGE_ALIGN_UP(incr) / PAGE_SIZE;

	//get the PMLT4
	uint64_t *PMLT4 = (uint64_t *)(proc->cr3 + kernel->hhdm);

	if(incr < 0){
		//make heap smaller
		for (int64_t i = 0; i > incr_pages; i--){
			uint64_t virt_page = (kernel->kheap.start + kernel->kheap.lenght)/PAGE_SIZE + i;
			uint64_t phys_page = (uint64_t)virt2phys((void *)(virt_page*PAGE_SIZE)) / PAGE_SIZE;
			unmap_page(PMLT4, virt_page);
			free_page(&kernel->bitmap,phys_page);
		}
	} else {
		//make heap bigger
		memseg_map(get_current_proc(),proc->heap_end,PAGE_SIZE * incr_pages,PAGING_FLAG_RW_CPL3 | PAGING_FLAG_NO_EXE);
	}
	proc->heap_end += incr_pages * PAGE_SIZE;
	return proc->heap_end;
}

int sys_ioctl(int fd,uint64_t request,void *arg){
	//first fd check
	if(!is_valid_fd(fd)){
		return -EBADF;
	}

	return vfs_ioctl(FD_GET(fd).node,request,arg);
}

int sys_usleep(useconds_t usec){
	micro_sleep(usec);
	return 0;
}

int sys_sleepuntil(struct timeval *time){
	sleep_until(*time);
	return 0;
}

int sys_gettimeofday(struct timeval *tv, struct timezone *tz){
	*tv = time;
	return 0;
}

int sys_pipe(int pipefd[2]){
	int read = find_fd();
	if(read == -1){
		return -ENXIO;
	}
	FD_GET(read).present = 1;
	FD_GET(read).flags = FD_READ;

	int write = find_fd();
	if(write == -1){
		FD_GET(read).present = 0;
		return -ENXIO;
	}
	FD_GET(write).flags = FD_WRITE;
	FD_GET(write).present = 1;

	create_pipe(&FD_GET(read).node,&FD_GET(write).node);

	pipefd[0] = read;
	pipefd[1] = write;
	return 0;
}

int sys_execve(const char *path,const char **argv){
	kdebugf("argv : %lx\n",argv);
	//get argc
	int argc = 0;
	while(argv[argc]){
		argc ++;
	}
	//FIXME : arg support 
	kdebugf("argc : %d\n",argc);
	kdebugf("try executing %s\n",path);
	return exec(path,argc,argv);
}

pid_t sys_fork(void){
	return fork();
}

int sys_mkdir(const char *path,mode_t mode){
	return vfs_mkdir(path,mode);
}

int sys_readdir(int fd,struct dirent *ret,long int index){
	if(!is_valid_fd(fd)){
		return -EBADF;
	}
	struct dirent *kret = vfs_readdir(FD_GET(fd).node,(uint64_t)index);
	
	if(kret == NULL){
		return -ENOENT;
	}

	//now copy kret to userspace ret and free it
	*ret = *kret;
	kfree(kret);
	return 0;
}

void node_stat(vfs_node *node,struct stat *st){
	//first get lasted info from filesystem
	vfs_sync(node);

	//then copy metadata
	st->st_size  = node->size;
	st->st_uid   = node->owner;
	st->st_gid   = node->group_owner;
	st->st_ctime = node->ctime;
	st->st_atime = node->atime;
	st->st_mtime = node->mtime;
	st->st_mode  = node->perm;

	st->st_nlink = 1;

	//file type to mode
	if(node->flags & VFS_FILE){
		st->st_mode |= S_IFREG;
	}
	if(node->flags & VFS_DIR){
		st->st_mode |= S_IFDIR;
	}
	if(node->flags & VFS_DEV){
		st->st_mode |= S_IFBLK; //well dev type don't exist let just say 
		st->st_mode |= S_IFCHR; //it is a block and char dev
	}
	if(node->flags & VFS_LINK){
		st->st_mode |= S_IFLNK;
	}

	//simulate fake blocks of 512 bytes
	//because blocks don't exist on stanix
	st->st_blksize = 512;
	st->st_blocks = (st->st_size + st->st_blksize - 1) / st->st_blksize;

	//set to 0 all the bloat that don't exist on stanix
	//and can't be emulated
	st->st_dev = 0;
	st->st_rdev = 0;
	st->st_ino = 0;
}

int sys_stat(const char *pathname,struct stat *st){
	vfs_node *node = vfs_open(pathname,VFS_READONLY);
	if(!node){
		return -ENOENT;
	}

	node_stat(node,st);

	return 0;
}

int sys_fstat(int fd,struct stat *st){
	if(!is_valid_fd(fd)){
		return -EBADF;
	}

	node_stat(FD_GET(fd).node,st);

	return 0;
}

pid_t sys_getpid(){
	return get_current_proc()->pid;
}

int sys_stub(void){
	return -ENOSYS;
}

void *syscall_table[] = {
	(void *)sys_exit,
	(void *)sys_open,
	(void *)sys_close,
	(void *)sys_read,
	(void *)sys_write,
	(void *)sys_seek,
	(void *)sys_dup,
	(void *)sys_dup2,
	(void *)sys_sbrk,
	(void *)sys_ioctl,
	(void *)sys_usleep,
	(void *)sys_sleepuntil,
	(void *)sys_gettimeofday,
	(void *)sys_stub, //settimeoftheday
	(void *)sys_pipe,
	(void *)sys_execve,
	(void *)sys_fork,
	(void *)sys_mkdir,
	(void *)sys_stub, //unlink
	(void *)sys_stub, //rmdir
	(void *)sys_readdir,
	(void *)sys_stat,
	(void *)sys_fstat,
	(void *)sys_getpid,
};

uint64_t syscall_number = sizeof(syscall_table) / sizeof(void *);
