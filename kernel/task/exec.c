#include <kernel/exec.h>
#include <kernel/scheduler.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <kernel/paging.h>
#include <kernel/print.h>
#include <kernel/userspace.h>
#include <kernel/memseg.h>
#include <kernel/sys.h>
#include <errno.h>
#include <elf.h>

int verfiy_elf(Elf64_Ehdr *header){
	if(memcmp(header,ELFMAG,4)){
		return 0;
	}

	if(header->e_ident[EI_VERSION] != EV_CURRENT){
		return 0;
	}

	#ifdef __i386__
	if(header->e_ident[EI_CLASS] != ELFCLASS32){
		return 0;
	}
	#else
	if(header->e_ident[EI_CLASS] != ELFCLASS64){
		return 0;
	}
	#endif

	return 1;
}

int exec(const char *path,int argc,const char **argv,int envc,const char **envp){
	int ret = 0;

	vfs_node *file = vfs_open(path,VFS_READONLY);

	if(!file){
		return -ENOENT;
	}

	//first read the header
	Elf64_Ehdr header;
	if(vfs_read(file,&header,0,sizeof(header)) < 0){
		ret = -ENOMEM;
		error:
		vfs_close(file);
		return ret;
	}

	//then verify it
	if(!verfiy_elf(&header)){
		ret = -ENOEXEC;
		goto error;
	}
	if(header.e_type != ET_EXEC){
		ret = -ENOEXEC;
		goto error;
	}
	
	//now read all program headers
	Elf64_Phdr *prog_header = kmalloc(header.e_phentsize * header.e_phnum);
	if(vfs_read(file,prog_header,header.e_phoff,header.e_phentsize * header.e_phnum) < 0){
		kfree(prog_header);
		goto error;
	}

	//save argv
	size_t total_arg_size = (argc + 1)  * sizeof(char *);
	char **saved_argv = kmalloc((argc + 1) * sizeof(char *));
	for (int i = 0; i < argc; i++){
		total_arg_size += strlen(argv[i]) + 1;
		saved_argv[i] = strdup(argv[i]);
	}
	saved_argv[argc] = NULL; // last NULL entry at the end

	//align this
	if(total_arg_size % 8){
		total_arg_size += 8 - (total_arg_size % 8);
	}

	//save envp
	total_arg_size += (envc + 1) * sizeof(char *);
	char **saved_envp = kmalloc((envc + 1) * sizeof(char *));
	for (int i = 0; i < envc; i++){
		total_arg_size += strlen(envp[i]) + 1;
		saved_envp[i] = strdup(envp[i]);
	}
	saved_envp[envc] = NULL; // last NULL entry at the end
	
	//unmap everything;
	list_node *current = get_current_proc()->memseg->frist_node;
	while(current){
		list_node *next = current->next;
		memseg_unmap(get_current_proc(),current->value);
		list_remove(get_current_proc()->memseg,current->value);
		current = next;
	}

	//cose fd with CLOEXEC flags
	for(size_t i = 0; i < MAX_FD; i++){
		if(FD_GET(i).present && (FD_GET(i).flags & FD_CLOEXEC)){
			sys_close(i);
		}
	}
	
	//set the heap start to 0 for future comparaison
	get_current_proc()->heap_start = 0;

	//then iterate trought each prog header
	for (size_t i = 0; i < header.e_phnum; i++){
		//only load porgram header with PT_LOAD
		if(prog_header[i].p_type != PT_LOAD){
			kdebugf("ignored header of type\n",prog_header[i].p_type);
			continue;
		}

		//convert elf header to paging header
		uint64_t flags = PAGING_FLAG_READONLY_CPL3;
		if(!(prog_header[i].p_flags & PF_X)){
			flags |= PAGING_FLAG_NO_EXE;
		}
		if(prog_header[i].p_flags & PF_W){
			flags |= PAGING_FLAG_RW_CPL3;
		}

		//make sure heap start is after the segment
		//so at the end the heap start is right after the end of excetuable
		if(get_current_proc()->heap_start < PAGE_ALIGN_UP(prog_header[i].p_vaddr + prog_header[i].p_memsz)){
			get_current_proc()->heap_start = PAGE_ALIGN_UP(prog_header[i].p_vaddr + prog_header[i].p_memsz);
		}
		
		memseg *seg;
		memseg_map(get_current_proc(),prog_header[i].p_vaddr,prog_header[i].p_memsz,PAGING_FLAG_RW_CPL0,MAP_PRIVATE|MAP_ANONYMOUS,NULL,0,&seg);
		memset((void*)prog_header[i].p_vaddr,0,prog_header[i].p_memsz);

		//file size must be <= to virtual size
		if(prog_header[i].p_filesz > prog_header[i].p_memsz){
			prog_header[i].p_filesz = prog_header[i].p_memsz;
		}

		if(vfs_read(file,(void*)prog_header[i].p_vaddr,prog_header[i].p_offset,prog_header[i].p_filesz) < 0){
			goto error;
		}

		//set the flags
		memseg_chflag(get_current_proc(),seg,flags);
	}
	kfree(prog_header);

	//check setuid /setgid bit
	struct stat st;
	if(vfs_getattr(file,&st) >= 0){
		if(st.st_mode & S_ISUID){
			get_current_proc()->suid =get_current_proc()->euid;
			get_current_proc()->uid  = st.st_uid;
			get_current_proc()->euid = st.st_uid;
		}
		if(st.st_mode & S_ISGID){
			get_current_proc()->sgid = get_current_proc()->egid;
			get_current_proc()->gid  = st.st_gid;
			get_current_proc()->egid = st.st_gid;
		}
	}

	vfs_close(file);

	//map stack
	memseg_map(get_current_proc(),USER_STACK_BOTTOM,USER_STACK_SIZE,PAGING_FLAG_RW_CPL3 | PAGING_FLAG_NO_EXE,MAP_PRIVATE|MAP_ANONYMOUS,NULL,0,NULL);

	//keep a one page guard between the executable and the heap
	get_current_proc()->heap_start += PAGE_SIZE;

	//set the heap end
	get_current_proc()->heap_end = get_current_proc()->heap_start;

	//make place for argv
	argv = (const char **)get_current_proc()->heap_start;
	sys_sbrk(PAGE_ALIGN_UP(total_arg_size));
	char *ptr = (char *)(((uintptr_t)argv) + (argc + 1) * sizeof(char *));
	

	//restore argv
	for (int i = 0; i < argc; i++){
		argv[i] = ptr;
		//copy saved arg to userpsace heap
		strcpy(ptr,saved_argv[i]);
		ptr += strlen(saved_argv[i]) + 1;
		kdebugf("arg %d : %s\n",i,saved_argv[i]);

		//free the saved arg in kernel space
		kfree(saved_argv[i]);
	}
	argv[argc] = NULL;

	//align the pointer
	if((uintptr_t)ptr % 8){
		ptr += 8 - ((uintptr_t)ptr % 8);
	}

	//restore envp
	envp = (const char **)ptr;
	ptr += (envc + 1) * sizeof(char *);
	for (int i = 0; i < envc; i++){
		envp[i] = ptr;
		//copy saved arg to userpsace heap
		strcpy(ptr,saved_envp[i]);
		ptr += strlen(saved_envp[i]) + 1;

		//free the saved env in kernel space
		kfree(saved_envp[i]);
	}
	envp[envc] = NULL;

	//free argv list
	kfree(saved_argv);
	//and envp too
	kfree(saved_envp);

	//reset signal handling of handled signals
	for(size_t i=0; i< sizeof(get_current_proc()->sig_handling)/sizeof(*get_current_proc()->sig_handling); i++){
		if(get_current_proc()->sig_handling[i].sa_handler != SIG_IGN){
			get_current_proc()->sig_handling[i].sa_handler = SIG_DFL;
		}
	}

	//now jump into the program !!
	kdebugf("exec entry : %p\n",header.e_entry);


	jump_userspace((void *)header.e_entry,(void *)USER_STACK_TOP,(uintptr_t)argc,(uintptr_t)argv,(uintptr_t)envc,(uintptr_t)envp);

	return 0;
}