#include <kernel/exec.h>
#include <kernel/scheduler.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <kernel/print.h>
#include <kernel/userspace.h>
#include <kernel/vmm.h>
#include <kernel/sys.h>
#include <errno.h>
#include <elf.h>

int sys_sbrk(size_t);

int verfiy_elf(Elf64_Ehdr *header) {
	if (memcmp(header, ELFMAG, 4)) {
		return 0;
	}

	if (header->e_ident[EI_VERSION] != EV_CURRENT) {
		return 0;
	}

#ifdef __i386__
	if (header->e_ident[EI_CLASS] != ELFCLASS32) {
		return 0;
	}
#else
	if (header->e_ident[EI_CLASS] != ELFCLASS64) {
		return 0;
	}
#endif

	return 1;
}

int exec_elf(const char *path, int argc, char **argv, int envc, char **envp, uintptr_t base, size_t depth) {
	int ret = 0;

	vfs_fd_t *file = vfs_open(path, O_RDONLY);

	if (!file) {
		return -ENOENT;
	}

	if (!(vfs_perm(file->inode) & PERM_EXECUTE)) {
		return -EACCES;
	}

	// first read the header
	Elf64_Ehdr header;
	if (vfs_read(file, &header, 0, sizeof(header)) < 0) {
		ret = -ENOMEM;
	error:
		vfs_close(file);
		return ret;
	}

	// then verify it
	if (!verfiy_elf(&header)) {
		ret = -ENOEXEC;
		goto error;
	}
	if (header.e_type != ET_EXEC && header.e_type != ET_DYN) {
		ret = -ENOEXEC;
		goto error;
	}
	if (header.e_type == ET_EXEC) {
		base = 0;
	} else {
		// for ET_DYN it must have an entry point
		if (!header.e_entry) {
			ret =-ENOEXEC;
			goto error;
		}
	}

	// now read all program headers
	Elf64_Phdr *prog_header = kmalloc(header.e_phentsize * header.e_phnum);
	if (vfs_read(file, prog_header, header.e_phoff, header.e_phentsize * header.e_phnum) < 0) {
		kfree(prog_header);
		goto error;
	}

	if (depth == 0) {
		// save argv
		char **saved_argv = kmalloc((argc + 1) * sizeof(char *));
		for (int i = 0; i < argc; i++) {
			saved_argv[i] = strdup(argv[i]);
		}
		saved_argv[argc] = NULL; // last NULL entry at the end

		// save envp
		char **saved_envp = kmalloc((envc + 1) * sizeof(char *));
		for (int i = 0; i < envc; i++) {
			saved_envp[i] = strdup(envp[i]);
		}
		saved_envp[envc] = NULL; // last NULL entry at the end
		argv = saved_argv;
		envp = saved_envp;

		vmm_unmap_all();

		// cose fd with CLOEXEC flags
		for (size_t i = 0; i < MAX_FD; i++) {
			file_descriptor_t file;
			if (get_fd(i, &file) < 0) continue;
			if (file.flags & FD_CLOEXEC) {
				close_fd(i);
			}
		}
	}

	// set the heap start to 0 for future comparaison
	get_current_proc()->heap_start = 0;

	// check setuid /setgid bit
	struct stat st;
	if (vfs_getattr(file->inode, &st) >= 0) {
		if (st.st_mode & S_ISUID) {
			get_current_proc()->suid =get_current_proc()->euid;
			get_current_proc()->uid  = st.st_uid;
			get_current_proc()->euid = st.st_uid;
		}
		if (st.st_mode & S_ISGID) {
			get_current_proc()->sgid = get_current_proc()->egid;
			get_current_proc()->gid  = st.st_gid;
			get_current_proc()->egid = st.st_gid;
		}
	}

	if (depth == 0) {
		// setup new cmdline and exe path
		set_cmdline(argv[0]);
		vfs_release_dentry(get_current_proc()->exe);
		get_current_proc()->exe = vfs_dup_dentry(file->dentry);
	}

	// then iterate trought each prog header
	for (size_t i = 0; i < header.e_phnum; i++) {
		if (prog_header[i].p_type == PT_INTERP) {
			char interp[PATH_MAX];
			size_t size = prog_header[i].p_filesz;
			if (size >= PATH_MAX) size = PATH_MAX - 1;
			vfs_read(file, interp, prog_header[i].p_offset, size);
			interp[size] = '\0';
			return exec_elf(interp, argc, argv, envc, envp, 0x100000000, depth + 1);
		}
		// only load porgram header with PT_LOAD
		if (prog_header[i].p_type != PT_LOAD) {
			kdebugf("ignored header of type\n", prog_header[i].p_type);
			continue;
		}

		// convert elf header to paging header
		long prot = MMU_FLAG_PRESENT | MMU_FLAG_USER;
		if (prog_header[i].p_flags & PF_R) {
			prot |= MMU_FLAG_READ;
		}
		if (prog_header[i].p_flags & PF_W) {
			prot |= MMU_FLAG_WRITE;
		}
		if (prog_header[i].p_flags & PF_X) {
			prot |= MMU_FLAG_EXEC;
		}

		// make sure heap start is after the segment
		// so at the end the heap start is right after the end of excetuable
		if (get_current_proc()->heap_start < PAGE_ALIGN_UP(prog_header[i].p_vaddr + prog_header[i].p_memsz)) {
			get_current_proc()->heap_start = PAGE_ALIGN_UP(prog_header[i].p_vaddr + prog_header[i].p_memsz);
		}

		if (prog_header[i].p_offset % PAGE_SIZE == prog_header[i].p_vaddr % PAGE_SIZE) {
			// we can use mmap
			
			// page align everything
			uintptr_t vaddr = PAGE_ALIGN_DOWN(prog_header[i].p_vaddr) + base;
			size_t vaddr_off = prog_header[i].p_vaddr % PAGE_SIZE;
			off_t offset = PAGE_ALIGN_DOWN(prog_header[i].p_offset);
			size_t filesz = PAGE_ALIGN_UP(prog_header[i].p_filesz + vaddr_off);
			size_t memsz  = PAGE_ALIGN_UP(prog_header[i].p_memsz + vaddr_off);
			size_t filesz_remainer = 0;

			if (prog_header[i].p_memsz > prog_header[i].p_filesz && (prog_header[i].p_filesz + vaddr_off) % PAGE_SIZE) {
				filesz_remainer = (prog_header[i].p_filesz + vaddr_off) % PAGE_SIZE;
			}

			if (filesz > 0) {
				vmm_seg_t *seg;
				if (vmm_map(vaddr, filesz, MMU_FLAG_WRITE | MMU_FLAG_PRESENT, VMM_FLAG_PRIVATE, file, offset, &seg) < 0) {
					goto error;
				}
				// zero the last page
				if (filesz_remainer) {
					kdebugf("%ld\n", filesz_remainer);
					uintptr_t start_bss = vaddr + filesz - PAGE_SIZE + filesz_remainer;
					memset((void*)start_bss, 0, PAGE_SIZE - filesz_remainer);
				}
				vmm_chprot(seg, prot);
			}
			if (memsz > filesz) {
				// we need to fill with anonymous mapping
				vaddr += filesz;
				if (vmm_map(vaddr, memsz - filesz, prot, VMM_FLAG_PRIVATE | VMM_FLAG_ANONYMOUS, NULL, 0, NULL) < 0) {
					goto error;
				}
			}
		} else {
			vmm_seg_t *seg;
			vmm_map(prog_header[i].p_vaddr, prog_header[i].p_memsz, MMU_FLAG_WRITE | MMU_FLAG_PRESENT, VMM_FLAG_PRIVATE | VMM_FLAG_ANONYMOUS, NULL, 0, &seg);

			//file size must be <= to virtual size
			if (prog_header[i].p_filesz > prog_header[i].p_memsz) {
				prog_header[i].p_filesz = prog_header[i].p_memsz;
			}

			if (vfs_read(file, (void *)prog_header[i].p_vaddr, prog_header[i].p_offset, prog_header[i].p_filesz) < 0) {
				goto error;
			}
			// set the protection
			vmm_chprot(seg, prot);
		}


	}
	kfree(prog_header);

	vfs_close(file);

	// map stack
	long stack_flags = MMU_FLAG_READ | MMU_FLAG_WRITE | MMU_FLAG_USER | MMU_FLAG_PRESENT;
	vmm_map(USER_STACK_BOTTOM, USER_STACK_SIZE, stack_flags, VMM_FLAG_ANONYMOUS | VMM_FLAG_PRIVATE, NULL, 0, NULL);

	// keep a one page guard between the executable and the heap
	get_current_proc()->heap_start += PAGE_SIZE;

	// set the heap end
	get_current_proc()->heap_end = get_current_proc()->heap_start;

	size_t total_arg_size = (argc+1) * sizeof(char*) + (envc+1) * sizeof(char*) + 16;
	for (int i=0; i<argc; i++) {
		total_arg_size += strlen(argv[i]) + 1;
	}
	for (int i=0; i<envc; i++) {
		total_arg_size += strlen(envp[i]) + 1;
	}

	// make place for argv
	char **user_argv = (char **)get_current_proc()->heap_start;
	sys_sbrk(PAGE_ALIGN_UP(total_arg_size));
	char *ptr = (char *)(((uintptr_t)user_argv) + (argc + 1) * sizeof(char *));

	// restore argv
	for (int i = 0; i < argc; i++) {
		user_argv[i] = ptr;
		// copy saved arg to userpsace heap
		strcpy(ptr, argv[i]);
		ptr += strlen(argv[i]) + 1;
		kdebugf("arg %d : %s\n", i, argv[i]);

		// free the saved arg in kernel space
		kfree(argv[i]);
	}
	user_argv[argc] = NULL;

	// align the pointer
	if ((uintptr_t)ptr % 8) {
		ptr += 8 - ((uintptr_t)ptr % 8);
	}

	// restore envp
	char **user_envp = (char **)ptr;
	ptr += (envc + 1) * sizeof(char *);
	for (int i = 0; i < envc; i++) {
		user_envp[i] = ptr;
		// copy saved arg to userpsace heap
		strcpy(ptr, envp[i]);
		ptr += strlen(envp[i]) + 1;

		// free the saved env in kernel space
		kfree(envp[i]);
	}
	user_envp[envc] = NULL;

	// free argv list
	kfree(argv);
	// and envp too
	kfree(envp);

	// reset signal handling of handled signals
	for (size_t i=0; i < sizeof(get_current_task()->sig_handling) / sizeof(*get_current_task()->sig_handling); i++) {
		if (get_current_task()->sig_handling[i].sa_handler != SIG_IGN) {
			get_current_task()->sig_handling[i].sa_handler = SIG_DFL;
		}
	}

	//now jump into the program !!
	kdebugf("exec entry : %p\n", header.e_entry);


	jump_userspace((void *)(header.e_entry + base), (void *)USER_STACK_TOP, (uintptr_t)argc, (uintptr_t)user_argv, (uintptr_t)envc, (uintptr_t)user_envp);

	return 0;
}

int exec(const char *path, int argc, const char **argv, int envc, const char **envp) {
	return exec_elf(path, argc, (char**)argv, envc, (char**)envp, 0x100000000, 0);
}
