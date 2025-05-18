#include <kernel/exec.h>
#include <kernel/scheduler.h>
#include <kernel/kernel.h>
#include <elf.h>
#include <kernel/string.h>
#include <kernel/paging.h>
#include <kernel/print.h>
#include <kernel/userspace.h>
#include <kernel/memseg.h>
#include <kernel/sys.h>
#include <errno.h>

int verfiy_elf(Elf64_Ehdr *header){
	if(memcmp(header,ELFMAG,4)){
		return 0;
	}

	if(header->e_ident[EI_VERSION] != EV_CURRENT){
		return 0;
	}

	if(header->e_ident[EI_CLASS] != ELFCLASS64){
		return 0;
	}

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
		saved_argv[i] = kmalloc(strlen(argv[i]) + 1);
		strcpy(saved_argv[i],argv[i]);
	}
	saved_argv[argc] = NULL; // last NULL entry at the end

	//align this
	if(total_arg_size & 0b111){
		total_arg_size += 8 - (total_arg_size % 8);
	}

	//save envp
	total_arg_size += (envc + 1) * sizeof(char *);
	char **saved_envp = kmalloc((envc + 1) * sizeof(char *));
	for (int i = 0; i < envc; i++){
		total_arg_size += strlen(envp[i]) + 1;
		saved_envp[i] = kmalloc(strlen(envp[i]) + 1);
		strcpy(saved_envp[i],envp[i]);
	}
	saved_envp[envc] = NULL; // last NULL entry at the end
	
	//unmap everything;
	memseg *current_memseg = get_current_proc()->first_memseg;
	while(current_memseg){
		memseg_unmap(get_current_proc(),current_memseg);
		current_memseg = current_memseg->next;
	}

	//cose fd with CLOEXEC flags
	for(size_t i = 0; i < MAX_FD; i++){
		if(FD_GET(i).present && (FD_GET(i).flags & FD_CLOEXEC)){
			sys_close(i);
		}
	}
	

	//set the heap start to 0
	get_current_proc()->heap_start = 0;

	//then iterate trought each prog header
	for (size_t i = 0; i < header.e_phnum; i++){
		//only load porgram header with PT_LOAD
		if(prog_header[i].p_type != PT_LOAD){
			continue;
		}

		//convert elf header to paging header
		uint64_t flags = PAGING_FLAG_READONLY_CPL3;
		if(!prog_header[i].p_flags & PF_X){
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
		
		memseg *seg = memseg_map(get_current_proc(),prog_header[i].p_vaddr,prog_header[i].p_memsz,PAGING_FLAG_RW_CPL0);
		memset((void*)prog_header[i].p_vaddr,0,prog_header[i].p_memsz);

		//file size must be <= to virtual size
		if(prog_header[i].p_filesz > prog_header[i].p_memsz){
			prog_header[i].p_filesz = prog_header[i].p_memsz;
		}

		if(vfs_read(file,(void*)prog_header[i].p_vaddr,prog_header[i].p_offset,prog_header[i].p_filesz) < 0){
			goto error;
		}

		//set the flags
		memeseg_chflag(get_current_proc(),seg,flags);
	}
	kfree(prog_header);

	vfs_close(file);

	//map stack
	memseg_map(get_current_proc(),USER_STACK_BOTTOM,USER_STACK_SIZE,PAGING_FLAG_RW_CPL3  | PAGING_FLAG_NO_EXE);

	//set the heap end
	get_current_proc()->heap_end = get_current_proc()->heap_start;

	//make place for argv
	argv = (const char **)get_current_proc()->heap_start;
	sys_sbrk(PAGE_ALIGN_UP(total_arg_size));
	char *ptr = (char *)(((uint64_t)argv) + (argc + 1) * sizeof(char *));
	

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
	if((uintptr_t)ptr & 0b111){
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

	//reset signal handling
	memset(get_current_proc()->sig_handling,0,sizeof(get_current_proc()->sig_handling));

	//now jump into the program !!
	kdebugf("exec entry : %p\n",header.e_entry);
	jump_userspace((void *)header.e_entry,(void *)USER_STACK_TOP,argc,argv,envc,envp);

	return 0;
}