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

static void push_long(void **stack, long value) {
	long *ptr = *stack;
	ptr--;
	*ptr = value;
	*stack = ptr;
}

static void push_auxv(void **stack, int type, long value) {
	push_long(stack, value);
	push_long(stack, type);
}

int exec_elf(const char *path, int argc, char **argv, int envc, char **envp, uintptr_t base, size_t depth, vfs_fd_t *interpret) {
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

			if (interpret) {
				vfs_close(file);
			} else {
				interpret = file;
			}
			
			kfree(prog_header);
			return exec_elf(interp, argc, argv, envc, envp, 0x100000000, depth + 1, interpret);
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

	void *sp = (void*)USER_STACK_TOP;
	
	// restore envp strings
	for (int i=envc-1; i>=0; i--) {
		size_t len = strlen(envp[i]) + 1;
		char *str = sp;
		str -= len;
		sp = str;
		strcpy(str, envp[i]);
	}

	// restore argv strings
	for (int i=argc-1; i>=0; i--) {
		size_t len = strlen(argv[i]) + 1;
		char *str = sp;
		str -= len;
		sp = str;
		strcpy(str, argv[i]);
	}


	// align stack on 16 bytes
	sp = (void*)((uintptr_t)sp & ~0xf);

	// calculate the amount of space taken by envp and argv
	// no need to calculate auxilary or NULL pointer because
	// auxiliary is alaways align (multiple of 2 ling)
	// the NULL of auxiliary go with argc
	// the NULL of envp go with the NULL of argv
	// so always a multiple of 8 bytes
	if ((argc + envc) % 2) {
		// we must add a padding long so the stack is 16 byte aligned at the end
		push_long(&sp, 0);
	}

	// push auxiliary vector
	push_long(&sp, 0);
	push_auxv(&sp, AT_BASE, base);
	push_auxv(&sp, AT_UID, get_current_proc()->uid);
	push_auxv(&sp, AT_EUID, get_current_proc()->euid);
	push_auxv(&sp, AT_GID, get_current_proc()->gid);
	push_auxv(&sp, AT_EGID, get_current_proc()->egid);

	if (interpret) {
		int fd = add_fd(interpret, 0);
		push_auxv(&sp, AT_EXECFD, fd);
	}
	
	// push envp
	uintptr_t ptr = USER_STACK_TOP;
	push_long(&sp, 0);
	for (int i=envc-1; i>=0; i--) {
		ptr -= strlen(envp[i]) + 1;
		kfree(envp[i]);
		push_long(&sp, ptr);
	}
	kfree(envp);

	// push argv
	push_long(&sp, 0);
	for (int i=argc-1; i>=0; i--) {
		ptr -= strlen(argv[i]) + 1;
		kfree(argv[i]);
		push_long(&sp, ptr);
	}
	kfree(argv);

	push_long(&sp, argc);

	// reset signal handling of handled signals
	for (size_t i=0; i < sizeof(get_current_task()->sig_handling) / sizeof(*get_current_task()->sig_handling); i++) {
		if (get_current_task()->sig_handling[i].sa_handler != SIG_IGN) {
			get_current_task()->sig_handling[i].sa_handler = SIG_DFL;
		}
	}

	//now jump into the program !!
	kdebugf("exec entry : %p\n", header.e_entry);
	
	jump_userspace((void *)(header.e_entry + base), sp, 0, 0, 0, 0);

	return 0;
}

int exec(const char *path, int argc, const char **argv, int envc, const char **envp) {
	return exec_elf(path, argc, (char**)argv, envc, (char**)envp, 0x100000000, 0, NULL);
}
