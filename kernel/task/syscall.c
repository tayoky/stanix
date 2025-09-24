#include <kernel/sys.h>
#include <kernel/scheduler.h>
#include <kernel/print.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/page.h>
#include <kernel/paging.h>
#include <kernel/time.h>
#include <kernel/pipe.h>
#include <kernel/exec.h>
#include <kernel/memseg.h>
#include <kernel/fork.h>
#include <kernel/userspace.h>
#include <kernel/string.h>
#include <kernel/arch.h>
#include <kernel/module.h>
#include <kernel/tty.h>
#include <kernel/signal.h>
#include <kernel/asm.h>
#include <sys/type.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <termios.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>

//we can't include stdlib
char *realpath(const char *path,char *res);

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
	if(!CHECK_STR(path)){
		return -EFAULT;
	}

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
	if(flags & O_NOFOLLOW){
		vfs_flags |= VFS_NOFOLOW;
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
			int result = vfs_create(path,mode & ~get_current_proc()->umask,VFS_FILE);

			if(result){
				//vfs_create failed
				return result;
			}

			node = vfs_open(path,vfs_flags);
			vfs_chown(node,get_current_proc()->euid,get_current_proc()->egid);
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
	if((flags & O_WRONLY || flags & O_RDWR || flags & O_TRUNC) && node->flags & VFS_DIR){
		vfs_close(node);
		return -EISDIR;
	}

	//now init the fd
	file->node = node;
	file->present = 1;
	file->offset = 0;

	//apply runc if needed
	if(flags & O_TRUNC){
		vfs_truncate(node,0);
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
	if(flags & O_NONBLOCK){
		file->flags |= FD_NONBLOCK;
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

ssize_t sys_write(int fd,void *buffer,size_t count){
	if(!CHECK_MEM(buffer,count)){
		return -EFAULT;
	}

	if((!is_valid_fd(fd) || (!FD_CHECK(fd,FD_WRITE)))){
		return -EBADF;
	}

	file_descriptor *file = &FD_GET(fd);

	//if non blocking mode check that write won't block
	if(FD_CHECK(fd,FD_NONBLOCK) && !(vfs_wait_check(file->node,POLLOUT) & POLLOUT)){
		return -EWOULDBLOCK;
	}

	//if append go to the end
	if(FD_CHECK(fd,FD_APPEND)){
		struct stat st;
		vfs_getattr(file->node,&st);
		file->offset = st.st_size;
	}

	int64_t wsize = vfs_write(file->node,buffer,file->offset,count);

	if(wsize > 0){
		file->offset += wsize;
	}

	return wsize;
}

ssize_t sys_read(int fd,void *buffer,size_t count){
	if(!CHECK_MEM(buffer,count)){
		return -EFAULT;
	}

	if((!is_valid_fd(fd) || (!FD_CHECK(fd,FD_READ)))){
		return -EBADF;
	}

	file_descriptor *file = &FD_GET(fd);
	
	//if non blocking mode check that read won't block
	if(FD_CHECK(fd,FD_NONBLOCK) && !(vfs_wait_check(file->node,POLLIN) & POLLIN)){
		return -EWOULDBLOCK;
	}

	int64_t rsize = vfs_read(file->node,buffer,file->offset,count);

	if(rsize > 0){
		file->offset += rsize;
	}

	return rsize;
}

void sys_exit(int error_code){
	//set that we exited normally
	get_current_proc()->exit_status = (1UL << 16) | error_code;
	kdebugf("exit with code : %ld\n",error_code);
	kill_proc();
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
		new_file->offset = old_file->offset;
	} else {
		return -EIO;
	}
	return newfd;
}

off_t sys_seek(int fd,int64_t offset,int whence){
	if(whence > 2){
		return -EINVAL;
	}

	if(!is_valid_fd(fd)){
		return -EBADF;
	}

	//get the fd
	file_descriptor *file = &FD_GET(fd);

	struct stat st;
	vfs_getattr(file->node,&st);

	switch (whence)
	{
	case SEEK_SET:
		file->offset = offset;
		break;
	case SEEK_CUR:
		file->offset += offset;
		break;
	case SEEK_END:
		file->offset = st.st_size + offset;
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

	intptr_t incr_pages = PAGE_ALIGN_UP(incr) / PAGE_SIZE;

	if(incr < 0){
		//make heap smaller
		for (int64_t i = 0; i > incr_pages; i--){
			uintptr_t virt_page = kernel->kheap.start + kernel->kheap.lenght + i * PAGE_SIZE;
			uintptr_t phys_page = (uintptr_t)virt2phys((void *)virt_page);
			unmap_page(get_current_proc()->addrspace, virt_page);
			pmm_free_page(phys_page);
		}
	} else {
		//make heap bigger
		memseg_map(get_current_proc(),proc->heap_end,PAGE_SIZE * incr_pages,PAGING_FLAG_RW_CPL3 | PAGING_FLAG_NO_EXE,MAP_PRIVATE|MAP_ANONYMOUS,NULL,0,NULL);
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

int sys_nanosleep(const struct timespec *duration,struct timespec *rem){
	if(!CHECK_STRUCT(duration)){
		return -EFAULT;
	}
	if(rem && !CHECK_STRUCT(rem)){
		return -EFAULT;
	}
	//TODO : set rem
	return micro_sleep(duration->tv_nsec / 1000 + duration->tv_sec * 1000000);
}

int sys_sleepuntil(struct timeval *time){
	if(!CHECK_STRUCT(time)){
		return -EFAULT;
	}
	sleep_until(*time);
	return 0;
}

 int sys_clock_gettime(clockid_t clockid,struct timespec *tp){
	if(!CHECK_STRUCT(tp)){
		return -EFAULT;
	}

	switch(clockid){
	case CLOCK_MONOTONIC:
	case CLOCK_REALTIME:
		tp->tv_sec  = time.tv_sec;
		tp->tv_nsec = time.tv_usec * 1000;
		return 0;
	default:
		return -EINVAL;
	}
}

int sys_pipe(int pipefd[2]){
	if(!CHECK_MEM(pipefd,sizeof(int) * 2)){
		return -EFAULT;
	}

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

int sys_execve(const char *path,const char **argv,const char **envp){
	if(!CHECK_STR(path)){
		return -EFAULT;
	}
	if(!CHECK_MEM(argv,sizeof(char *))){
		return -EFAULT;
	}
	if(!CHECK_MEM(envp,sizeof(char *))){
		return -EFAULT;
	}

	//get argc
	int argc = 0;
	while(argv[argc]){
		if(!CHECK_STR(argv[argc])){
			return -EFAULT;
		}
		argc ++;

		if(!CHECK_MEM(&argv[argc],sizeof(char*))){
			return -EFAULT;
		}
	}

	//get envc
	int envc = 0;
	while(envp[envc]){
		if(!CHECK_STR(envp[envc])){
			return -EFAULT;
		}
		envc ++;

		if(!CHECK_MEM(&envp[envc],sizeof(char*))){
			return -EFAULT;
		}
	}
	kdebugf("try executing %s\n",path);
	return exec(path,argc,argv,envc,envp);
}

pid_t sys_fork(void){
	return fork();
}

int sys_mkdir(const char *path,mode_t mode){
	if(!CHECK_STR(path)){
		return -EFAULT;
	}

	//remove slash at the endd, as the vfs don't really like them
	char *kpath = strdup(path);
	for(char *ptr=kpath+strlen(kpath)-1; ptr>kpath && *ptr == '/'; ptr--)*ptr = '\0';

	int ret = vfs_mkdir(kpath,mode & ~get_current_proc()->umask);
	kfree(kpath);
	return ret;
}

int sys_readdir(int fd,struct dirent *ret,long int index){
	if(!CHECK_STRUCT(ret)){
		return -EFAULT;
	}

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

int sys_stat(const char *pathname,struct stat *st){
	if(!CHECK_STR(pathname)){
		return -EFAULT;
	}
	if(!CHECK_STRUCT(st)){
		return -EFAULT;
	}
	
	vfs_node *node = vfs_open(pathname,VFS_READONLY);
	if(!node){
		return -ENOENT;
	}

	int ret = vfs_getattr(node,st);

	vfs_close(node);
	return ret;
}

int sys_lstat(const char *pathname,struct stat *st){
	if(!CHECK_STR(pathname)){
		return -EFAULT;
	}
	if(!CHECK_STRUCT(st)){
		return -EFAULT;
	}
	
	vfs_node *node = vfs_open(pathname,VFS_READONLY | VFS_NOFOLOW);
	if(!node){
		return -ENOENT;
	}

	int ret = vfs_getattr(node,st);

	vfs_close(node);
	return ret;
}

int sys_fstat(int fd,struct stat *st){
	if(!CHECK_STRUCT(st)){
		return -EFAULT;
	}
	
	if(!is_valid_fd(fd)){
		return -EBADF;
	}

	vfs_getattr(FD_GET(fd).node,st);

	return 0;
}

int sys_getcwd(char *buf,size_t size){
	if(!CHECK_MEM(buf,size)){
		return -EFAULT;
	}

	if(size < strlen(get_current_proc()->cwd_path) + 1){
		return -ERANGE;
	}
	strcpy(buf,get_current_proc()->cwd_path);

	return 0;
}

int sys_chdir(const char *path){
	if(!CHECK_STR(path)){
		return -EFAULT;
	}

	//check if exist
	vfs_node *node = vfs_open(path,VFS_READONLY);
	if(!node){
		return -ENOENT;
	}

	if(!(node->flags & VFS_DIR)){
		return -ENOTDIR;
	}


	//make the cwd absolute
	char *cwd = realpath(path,NULL);

	//cwd should never finish with /
	if(cwd[strlen(cwd) - 1] == '/'){
		cwd[strlen(cwd) - 1] = '\0';
	}

	//free old cwd
	kfree(get_current_proc()->cwd_path);
	vfs_close(get_current_proc()->cwd_node);

	//set new cwd
	get_current_proc()->cwd_node = node;
	get_current_proc()->cwd_path = cwd;

	return 0;
}

int sys_waitpid(pid_t pid,int *status,int options){
	if(status && !CHECK_MEM(status,sizeof(status))){
		return -EFAULT;
	}

	if(pid == 0) pid = -get_current_proc()->group;

	//prevent child state from changing
	kernel->can_task_switch = 0;

	kdebugf("wait for %ld\n",pid);

	if(pid == -1){
		//wait for any
		foreach(node,get_current_proc()->child){
			//don't wait for zombie
			process *proc = node->value;
			if(proc->flags & PROC_FLAG_ZOMBIE){
				if(status){
					*status = proc->exit_status;
				}
				kernel->can_task_switch = 1;
				pid = proc->pid;
				final_proc_cleanup(proc);
				return pid;
			}
		}
	} else {
		//wait for pid
		process *proc = pid2proc(pid);

		//make sure it exist and is a child
		if((!proc) || proc->parent != get_current_proc()){
			kernel->can_task_switch = 1;
			return -ECHILD;
		}

		//don't wait for zombie
		if(proc->flags & PROC_FLAG_ZOMBIE){
			if(status){
				*status = proc->exit_status;
			}
			kernel->can_task_switch = 1;
			final_proc_cleanup(proc);
			return pid;
		}
	}

	if(options & WNOHANG){
		return -ECHILD;
	}
	
	get_current_proc()->waitfor = pid;
	get_current_proc()->flags |= PROC_FLAG_WAIT;
	
	//block, when we wake up the process is now a zombie
	if(block_proc() == -EINTR){
		return -EINTR;
	}

	process *proc = get_current_proc()->waker;

	//get the exit status
	if(status){
		*status = proc->exit_status;
	}

	pid = proc->pid;

	final_proc_cleanup(proc);

	return pid;
}

int sys_unlink(const char *pathname){
	if(!CHECK_STR(pathname)){
		return -EFAULT;
	}

	//unlink don't work on dir while vfs_unlink work on dir
	vfs_node *node = vfs_open(pathname,VFS_READONLY);
	if(!node){
		return -ENOENT;
	}
	if(node->flags & VFS_DIR){
		vfs_close(node);
		return -EISDIR;
	}
	vfs_close(node);

	return vfs_unlink(pathname);
}

int sys_rmdir(const char *pathname){
	if(!CHECK_STR(pathname)){
		return -EFAULT;
	}

	//check for dir and empty
	vfs_node *node = vfs_open(pathname,VFS_READONLY);
	if(!node){
		return -ENOENT;
	}
	if(!(node->flags & VFS_DIR)){
		vfs_close(node);
		return -ENOTDIR;
	}
	struct dirent *entry = vfs_readdir(node,2);
	if(entry){
		kfree(entry);
		vfs_close(node);
		return -ENOTEMPTY;
	}

	//now check if it in use
	if(node->ref_count > 1){
		vfs_close(node);
		return -EBUSY;
	}
	vfs_close(node);

	return vfs_unlink(pathname);
}

int sys_insmod(const char *pathname,const char **arg){
	if(!CHECK_STR(pathname)){
		return -EFAULT;
	}
	if(!CHECK_MEM(arg,sizeof(char *))){
		return -EFAULT;
	}

	if(get_current_proc()->euid != EUID_ROOT){
		return -EPERM;
	}

	//check arg
	int argc = 0;
	while(arg[argc]){
		if(!CHECK_STR(arg[argc])){
			return -EFAULT;
		}
		argc ++;
		if(!CHECK_MEM(&arg[argc],sizeof(char*))){
			return -EFAULT;
		}
	}

	return insmod(pathname,arg,NULL);
}

int sys_rmmod(const char *name){
	if(!CHECK_STR(name)){
		return -EFAULT;
	}

	if(get_current_proc()->euid != EUID_ROOT){
		return -EPERM;
	}

	return rmmod(name);
}

int sys_isatty(int fd){
	if(!is_valid_fd(fd)){
		return -EBADF;
	}

	if(FD_GET(fd).node->flags & VFS_TTY){
		return 1;
	} else {
		return -ENOTTY;
	}
}

int sys_openpty(int *amaster, int *aslave, char *name,const struct termios *termp,const struct winsize *winp){
	if((termp && !CHECK_STRUCT(termp)) || (winp && !CHECK_STRUCT(winp)) || (name && !CHECK_MEM(name,PATH_MAX))){
		return -EFAULT;
	}
	if((!CHECK_STRUCT(amaster)) || (!CHECK_STRUCT(aslave))){
		return -EFAULT;
	}

	int master = find_fd();
	if(master == -1){
		return -ENXIO;
	}
	FD_GET(master).present = 1;
	FD_GET(master).flags = FD_WRITE | FD_READ;

	int slave = find_fd();
	if(slave == -1){
		FD_GET(master).present = 0;
		return -ENXIO;
	}
	FD_GET(slave).flags = FD_WRITE |FD_READ;
	FD_GET(slave).present = 1;
	*amaster = master;
	*aslave = slave;

	tty *tty;

	int ret = new_pty(&FD_GET(master).node,&FD_GET(slave).node,&tty);
	if(ret < 0){
		return ret;
	}

	if(termp){
		tty->termios = *termp;
	}
	if(winp){
		tty->size = *winp;
	}
	if(name){
		sprintf(name,"/dev/pts/%d",ret);
	}

	return 0;
}

int sys_poll(struct pollfd *fds, nfds_t nfds, int timeout){
	if(!CHECK_MEM(fds,sizeof(struct pollfd) * nfds)){
		return -EFAULT;
	}

	struct timeval end;
	if(timeout >= 0){
		end.tv_usec = time.tv_usec;
		end.tv_sec  = time.tv_sec;

		end.tv_sec = timeout / 1000;
		timeout %= 1000;
		
		end.tv_usec += timeout * 1000;

		if(end.tv_usec > 1000000){
			end.tv_usec -= 1000000;
			end.tv_sec++;
		}
	}
	
	for(;;){
		int ready_count = 0;
		for(nfds_t i=0; i<nfds; i++){
			fds[i].revents = 0;
			if(!is_valid_fd(fds[i].fd)){
				fds[i].revents = POLLNVAL;
				return -EBADF;
			}
			int r = vfs_wait_check(FD_GET(fds[i].fd).node,fds[i].events);
			if(r){
				//it's ready !!!!
				fds[i].revents = r;
				ready_count++;
			}
		}
		if(ready_count){
			return ready_count;
		}

		//if timeout expire exit
		if(timeout >= 0 && (time.tv_sec < end.tv_sec || (time.tv_sec == end.tv_sec && time.tv_usec <= end.tv_usec))){
			break;
		}
		yield(1);
	}
	
	return 0;
}

int sys_sigprocmask(int how, const sigset_t * restrict set, sigset_t *oldset){
	if(set && !CHECK_STRUCT(set)){
		return -EFAULT;
	}
	if(oldset && !CHECK_STRUCT(oldset)){
		return -EFAULT;
	}

	if(oldset){
		*oldset = get_current_proc()->sig_mask;
	}

	//if set is null then we have finished our jobs
	if(!set){
		return 0;
	}

	switch(how){
	case SIG_BLOCK:
		get_current_proc()->sig_mask |= *set;
		break;
	case SIG_UNBLOCK:
		get_current_proc()->sig_mask &= ~*set;
		break;
	case SIG_SETMASK:
		get_current_proc()->sig_mask = *set;
		break;
	default :
		return -EINVAL;
	}
	return 0;
}

int sys_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact){
	if(act && !CHECK_STRUCT(act)){
		return -EFAULT;
	}
	if(oldact && !CHECK_STRUCT(oldact)){
		return -EFAULT;
	}

	if(signum >= _NSIG || signum <= 0){
		return -EINVAL;
	}

	if(oldact){
		*oldact = get_current_proc()->sig_handling[signum];
	}

	//can't change handling for SIGKILL and SIGSTOP
	if(signum == SIGKILL || signum == SIGSTOP){
		return 0;
	}

	if(act){
		get_current_proc()->sig_handling[signum] = *act;
	}

	return 0;
}

int sys_sigpending(sigset_t *set){
	if(!CHECK_STRUCT(set)){
		return -EFAULT;
	}

	*set = get_current_proc()->pending_sig;

	return 0;
}

int sys_kill(pid_t pid,int sig){
	if(pid < 0){
		if(send_sig_pgrp(-pid,sig) < 0){
			return -ESRCH;
		}
		return 0;
	}
	process *proc = pid2proc(pid);
	if(!proc){
		return -ESRCH;
	}

	//TODO : permission checks

	send_sig(proc,sig);

	return 0;
}

pid_t sys_getpid(){
	return get_current_proc()->pid;
}

int sys_mount(const char *source, const char *target,const char *filesystemtype, unsigned long mountflags,const void *data){
	if((!CHECK_STR(source)) || (!CHECK_STR(target)) || (!CHECK_STR(filesystemtype))){
		return -EFAULT;
	}
	if(data && !CHECK_PTR(data)){
		return -EFAULT;
	}

	if(get_current_proc()->euid != EUID_ROOT){
		return -EPERM;
	}

	return vfs_auto_mount(source,target,filesystemtype,mountflags,data);
}

void *sys_mmap(uintptr_t addr,size_t length,int prot,int flags,int fd,off_t offset){
	int check = flags & (MAP_SHARED | MAP_PRIVATE);
	if(check == 0 || check == (MAP_PRIVATE & MAP_SHARED))return (void*)-EINVAL;
	if(!length)return (void *)-EINVAL;

	if(flags & MAP_FIXED){
		if(!CHECK_PTR_INRANGE(addr + length))return (void *)-EEXIST;
		if(addr % PAGE_SIZE || length % PAGE_SIZE) return (void *)-EINVAL;
	} else {
		addr = 0;
	}

	uint64_t pflags = PAGING_FLAG_READONLY_CPL3;
	if(prot & PROT_WRITE){
		pflags |= PAGING_FLAG_RW_CPL3;
	}
	if(!(prot & PROT_EXEC)){
		pflags |= PAGING_FLAG_NO_EXE;
	}

	vfs_node *node;
	if(!(flags & MAP_ANONYMOUS)){
		if(!FD_GET(fd).present){
			return (void *)-EBADF;
		}
		node = FD_GET(fd).node;
	}


	memseg *seg;
	int ret = memseg_map(get_current_proc(),addr,length,pflags,flags,node,offset,&seg);
	if(ret < 0){
		return (void*)(uintptr_t)ret;
	} else {
		return (void*)seg->addr;
	}
}

int sys_munmap(void *addr, size_t len){
	if(!len)return -EINVAL;
	if(!CHECK_PTR_INRANGE((uintptr_t)addr + len))return -EINVAL;
	foreach(node,get_current_proc()->memseg){
		memseg *seg = node->value;
		if(seg->addr >= (uintptr_t)addr && seg->addr + seg->addr <= (uintptr_t)addr + len){
			memseg_unmap(get_current_proc(),seg);
		}
	}
	return 0;
}

int sys_setuid(uid_t uid){
	if(get_current_proc()->euid == EUID_ROOT){
		get_current_proc()->uid = uid;
		get_current_proc()->euid = uid;
		get_current_proc()->suid = uid;
		return 0;
	} else if(uid == get_current_proc()->uid || uid == get_current_proc()->suid){
		get_current_proc()->euid = uid;
		return 0;
	} else {
		return -EPERM;
	}
}
int sys_seteuid(uid_t uid){
	if(get_current_proc()->euid == EUID_ROOT || uid == get_current_proc()->uid || uid == get_current_proc()->suid){
		get_current_proc()->euid = uid;
		return 0;
	}
	return -EPERM;
}
uid_t sys_getuid(void){
	return get_current_proc()->uid;
}
uid_t sys_geteuid(void){
	return get_current_proc()->euid;
}

int sys_setgid(gid_t gid){
	if(get_current_proc()->euid == EUID_ROOT){
		get_current_proc()->gid = gid;
		get_current_proc()->egid = gid;
		get_current_proc()->sgid = gid;
		return 0;
	} else if(gid == get_current_proc()->gid || gid == get_current_proc()->sgid){
		get_current_proc()->egid = gid;
		return 0;
	} else {
		return -EPERM;
	}
}
int sys_setegid(gid_t gid){
	if(get_current_proc()->euid == EUID_ROOT || gid == get_current_proc()->gid || gid == get_current_proc()->sgid){
		get_current_proc()->egid = gid;
		return 0;
	}
	return -EPERM;
}
uid_t sys_getgid(void){
	return get_current_proc()->gid;
}
uid_t sys_getegid(void){
	return get_current_proc()->egid;
}

static int chmod_node(vfs_node *node,mode_t mode){
	struct stat st;
	vfs_getattr(node,&st);
	if(st.st_uid != get_current_proc()->euid && get_current_proc()->euid != EUID_ROOT){
		return -EPERM;
	}
	return vfs_chmod(node,mode);
}

int sys_chmod(const char *pathname, mode_t mode){
	vfs_node *node = vfs_open(pathname,VFS_WRITEONLY);
	if(!node)return -ENOENT;

	int ret = chmod_node(node,mode);
	vfs_close(node);

	return ret;
}

int sys_lchmod(const char *pathname, mode_t mode){
	vfs_node *node = vfs_open(pathname,VFS_WRITEONLY | VFS_NOFOLOW);
	if(!node)return -ENOENT;

	int ret = chmod_node(node,mode);
	vfs_close(node);

	return ret;
}

int sys_fchmod(int fd, mode_t mode){
	if(!is_valid_fd(fd)){
		return -EBADF;
	}

	return chmod_node(FD_GET(fd).node,mode);
}

static int chown_node(vfs_node *node,uid_t owner,gid_t group){
	struct stat st;
	vfs_getattr(node,&st);
	if(st.st_uid != get_current_proc()->euid && get_current_proc()->euid != EUID_ROOT){
		return -EPERM;
	}
	st.st_uid = owner;
	st.st_gid = group;
	st.st_mode &= ~(S_ISUID | S_ISGID);
	return vfs_setattr(node,&st);
}

int sys_chown(const char *pathname, uid_t owner, gid_t group){
	vfs_node *node = vfs_open(pathname,VFS_WRITEONLY);
	if(!node)return -ENOENT;

	int ret = chown_node(node,owner,group);
	vfs_close(node);

	return ret;
}

int sys_lchown(const char *pathname, uid_t owner, gid_t group){
	vfs_node *node = vfs_open(pathname,VFS_WRITEONLY | VFS_NOFOLOW);
	if(!node)return -ENOENT;

	int ret = chown_node(node,owner,group);
	vfs_close(node);

	return ret;
}

int sys_fchown(int fd, uid_t owner, gid_t group){
	if(!is_valid_fd(fd)){
		return -EBADF;
	}

	return chown_node(FD_GET(fd).node,owner,group);
}

//TODO : check if group exist
int sys_setpgid(pid_t pid, pid_t pgid){
	kdebugf("setpgid %ld %ld\n",pid,pgid);
	if(pid == 0)pid = get_current_proc()->pid;
	if(pgid == 0)pgid = get_current_proc()->pid;
	if(pgid < 0){
		return -EINVAL;
	}
	process *proc = pid2proc(pid);
	if(!proc || (proc->parent != get_current_proc() && proc != get_current_proc())){
		return -ESRCH;
	}
	if(proc->sgid != get_current_proc()->sid){
		return -EPERM;
	}
	proc->group = pgid;
	return 0;
}

pid_t sys_getpgid(pid_t pid){
	process *proc = pid2proc(pid);
	if(!proc || (proc->parent != get_current_proc() && proc != get_current_proc())){
		return -ESRCH;
	}
	return proc->group;
}

int sys_fcntl(int fd, int op, int arg){
	if(!is_valid_fd(fd)){
		return -EBADF;
	}

	switch(op){
	case F_DUPFD:
		//TODO respect arg
		return sys_dup(fd);
	case F_GETFD:
		return FD_GET(fd).flags & ~0xFUL;
	case F_SETFD:
		FD_GET(fd).flags = (FD_GET(fd).flags & 0xFUL) | (arg & ~0xFUL);
		return 0;
	default:
		return -EINVAL;
	}
}

mode_t sys_umask(mode_t mask){
	mode_t old = get_current_proc()->umask;
	get_current_proc()->umask = mask;
	return old;
}

int sys_access(const char *pathname, int mode){
	uint64_t flags = VFS_READONLY;
	if(mode & W_OK)flags |= VFS_WRITEONLY;
	vfs_node *node = vfs_open(pathname,flags);
	if(!node)return -ENOENT;
	vfs_close(node);
	return 0;
}

int sys_truncate(const char *path, off_t length){
	vfs_node *node = vfs_open(path,VFS_WRITEONLY);
	if(!node)return -ENOENT;
	int ret = vfs_truncate(node,(size_t)length);
	vfs_close(node);
	return ret;
}

int sys_ftruncate(int fd, off_t length){
	//must be open for writing
	if(!is_valid_fd(fd) ||!FD_CHECK(fd,VFS_WRITEONLY)){
		return -EBADF;
	}

	return vfs_truncate(FD_GET(fd).node,(size_t)length);
}

int sys_link(const char *oldpath, const char *newpath){
	if(!CHECK_STR(oldpath) || !CHECK_STR(newpath)){
		return -EFAULT;
	}

	return vfs_link(oldpath,newpath);
}

int sys_rename(const char *oldpath, const char *newpath){
	if(!CHECK_STR(oldpath) || !CHECK_STR(newpath)){
		return -EFAULT;
	}
	//only supprot rename inside same fs sadly
	//TODO/FIXME : need to check if inside same fs
	int ret = vfs_link(oldpath,newpath);
	if(ret < 0)return ret;
	return vfs_unlink(oldpath);
}

int sys_symlink(const char *target, const char *linkpath){
	if(!CHECK_STR(target) || !CHECK_STR(linkpath)){
		return -EFAULT;
	}
	return vfs_symlink(target,linkpath);
}

ssize_t sys_readlink(const char *path,char *buf, size_t bufsize){
	if(!CHECK_STR(path)){
		return -EFAULT;
	}
	if(!CHECK_MEM(buf,bufsize)){
		return -EFAULT;
	}

	vfs_node *node = vfs_open(path,VFS_READONLY | VFS_NOFOLOW);
	if(!node){
		return -ENOENT;
	}

	ssize_t ret = vfs_readlink(node,buf,bufsize);
	vfs_close(node);
	return ret;
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
	(void *)sys_nanosleep,
	(void *)sys_sleepuntil,
	(void *)sys_clock_gettime,
	(void *)sys_stub, //clock_settime
	(void *)sys_pipe,
	(void *)sys_execve,
	(void *)sys_fork,
	(void *)sys_mkdir,
	(void *)sys_unlink,
	(void *)sys_rmdir,
	(void *)sys_readdir,
	(void *)sys_stat,
	(void *)sys_fstat,
	(void *)sys_getcwd,
	(void *)sys_chdir,
	(void *)sys_waitpid,
	(void *)sys_insmod,
	(void *)sys_rmmod,
	(void *)sys_isatty,
	(void *)sys_openpty,
	(void *)sys_poll,
	(void *)sys_sigprocmask,
	(void *)sys_sigaction,
	(void *)sys_stub, //sys_sigwait
	(void *)sys_stub, //sys_sigsuspend
	(void *)sys_sigpending,
	(void *)sys_kill,
	(void *)sys_getpid,
	(void *)sys_mount,
	(void *)sys_stub, //sys_umount
	(void *)sys_mmap,
	(void *)sys_munmap,
	(void *)sys_stub, //sys_mprotect
	(void *)sys_stub, //sys_msync
	(void *)sys_setuid,
	(void *)sys_seteuid,
	(void *)sys_getuid,
	(void *)sys_geteuid,
	(void *)sys_setgid,
	(void *)sys_setegid,
	(void *)sys_getgid,
	(void *)sys_getegid,
	(void *)sys_chmod,
	(void *)sys_fchmod,
	(void *)sys_chown,
	(void *)sys_fchown,
	(void *)sys_setpgid,
	(void *)sys_getpgid,
	(void *)sys_fcntl,
	(void *)sys_umask,
	(void *)sys_access,
	(void *)sys_stub,   //sys_utimes
	(void *)sys_truncate,
	(void *)sys_ftruncate,
	(void *)sys_link,
	(void *)sys_rename,
	(void *)sys_lstat,
	(void *)sys_lchmod,
	(void *)sys_lchown,
	(void *)sys_symlink,
	(void *)sys_readlink,
};

uint64_t syscall_number = sizeof(syscall_table) / sizeof(void *);

void syscall_handler(fault_frame *context){
	enable_interrupt();
	if(ARG0_REG(*context) >= syscall_number){
		ARG0_REG(*context) = (uint64_t)-EINVAL;
		return;
	}

	get_current_proc()->syscall_frame = context;

	long (*syscall)(long,long,long,long,long) = syscall_table[ARG0_REG(*context)];
	RET_REG(*context) = syscall(ARG1_REG(*context),ARG2_REG(*context),ARG3_REG(*context),ARG4_REG(*context),ARG5_REG(*context));

	//now handle any unlblocked pending syscall
	handle_signal(context);
}