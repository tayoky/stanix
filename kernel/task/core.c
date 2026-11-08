#include <kernel/core.h>
#include <kernel/process.h>
#include <kernel/string.h>
#include <kernel/signal.h>
#include <kernel/vmm.h>
#include <kernel/vfs.h>
#include <kernel/mmu.h>
#include <kernel/elf.h>

// core dump

typedef struct note {
	size_t size;
	const char *name;
	void *data;
	int type;
} note_t;

static int core_write_exact(vfs_fd_t *fd, const void *buffer, size_t size) {
	ssize_t w = vfs_user_write(fd, buffer, size);
	if (w < 0) return w;
	if (w < (ssize_t)size) return -EIO;
	return 0;
}

static int core_write_padded4(vfs_fd_t *fd, const void *buffer, size_t size) {
	int ret = core_write_exact(fd, buffer, size);
	if (ret < 0) return ret;
	if (size % 4) {
		static char padding[4] = {0};
		ret = core_write_exact(fd, padding, 4 - (size % 4));
		if (ret < 0) return ret;
	}
	return 0;
}

static int core_write_note(vfs_fd_t *fd, note_t *note) {
	Elf_Nhdr note_header = {
		.n_namesz = note->name ? strlen(note->name) + 1 : 0,
		.n_descsz = note->size,
		.n_type   = note->type,
	};

	int ret = core_write_exact(fd, &note_header, sizeof(note_header));
	if (ret < 0) return ret;

	if (note->name) {
		ret = core_write_padded4(fd, note->name, strlen(note->name) + 1);
		if (ret < 0) return ret;
	}

	return core_write_padded4(fd, note->data, note->size);
}

static size_t core_pad4(size_t n) {
	return (n + 3) & ~3;
}

static size_t core_note_size(note_t *note) {
	size_t size = sizeof(Elf_Nhdr);
	if (note->name) size += core_pad4(strlen(note->name) + 1);
	size += core_pad4(note->size);
	return size;
}

static void core_free_note(note_t *note) {
	kfree(note->data);
}

typedef struct files_note_desc {
	long start;
	long end;
	long file_off;
} files_note_desc_t;

typedef struct files_note {
	long count;
	long page_size;
	files_note_desc_t descs[];
} files_note_t;

static int core_fill_files_note(note_t *note, size_t segs_count) {
	size_t files_size = sizeof(files_note_t) + segs_count * (sizeof(files_note_desc_t) + PATH_MAX);
	files_note_t *data = kmalloc(files_size);
	if (!data) return -ENOMEM;

	size_t i = 0;
	char *name_start = (char*)data + sizeof(files_note_t) + segs_count * sizeof(files_note_desc_t);
	char *name_pos = name_start;
	avltree_foreach (node, &vmm_get_current_space()->segs) {
		vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
		char *name = seg->fd ? vfs_dentry_path(seg->fd->dentry) : NULL;
		if (!name) continue;
		if (strlen(name) > PATH_MAX - 1) {
			kfree(name);
			continue;
		}

		size_t name_size = strlen(name) + 1;
		memcpy(name_pos, name, name_size);
		name_pos += name_size;

		kfree(name);

		data->descs[i].start    = seg->start;
		data->descs[i].end      = seg->end;
		data->descs[i].file_off = seg->offset;
		i++;
	}
	
	data->count = i;
	data->page_size = PAGE_SIZE;

	// move the names list down, right after the descriptors
	char *descs_end = (char*)data + sizeof(files_note_t) + i * sizeof(files_note_desc_t);
	memmove(descs_end, name_start, name_pos - name_start);
	files_size = descs_end - (char*)data + name_pos - name_start;

	note->name = "CORE";
	note->type = NT_FILE;
	note->size = files_size;
	note->data = data;
	return 0;
}

static int core_raw_write(vfs_fd_t *fd, siginfo_t *siginfo, vmm_space_t *space) {
	// first pass, count segments
	size_t segs_count = 0;
	avltree_foreach (node, &space->segs) {
		segs_count++;
	}

	// PT_NOTE + one PT_LOAD per seg
	size_t phdrs_count = segs_count + 1;

	// one section per seg + note + null + strtab
	size_t sections_count = segs_count + 3;
	Elf_Ehdr header = {
		.e_ident = {
			[EI_MAG0] = ELFMAG0,
			[EI_MAG1] = ELFMAG1,
			[EI_MAG2] = ELFMAG2,
			[EI_MAG3] = ELFMAG3,
			[EI_CLASS] = ELFCLASS,
			[EI_DATA] = ELFDATA,
			[EI_VERSION] = EV_CURRENT,
		},
		.e_type    = ET_CORE,
		.e_machine = ELFMACHINE,
		.e_version = EV_CURRENT,
		.e_phoff = sizeof(header),
		.e_shoff = sizeof(header) + phdrs_count * sizeof(Elf_Phdr),
		.e_ehsize = sizeof(header),
		.e_phentsize = sizeof(Elf_Phdr),
		.e_phnum  = phdrs_count,
		.e_shentsize = sizeof(Elf_Shdr),
		.e_shnum  = sections_count,
		.e_shstrndx = sections_count - 1,
	};

	int ret = core_write_exact(fd, &header, sizeof(header));
	if (ret < 0) return ret;

	note_t files_note;
	ret = core_fill_files_note(&files_note, segs_count);
	if (ret < 0) return ret;
	
	static const char strtab[] = "\0.shstrtab\0note0\0load";
#define CORE_NAME_SHSTRTAB 1
#define CORE_NAME_NOTE0    CORE_NAME_SHSTRTAB + 10
#define CORE_NAME_LOAD     CORE_NAME_NOTE0 + 6
	off_t strtab_offset = sizeof(header) + phdrs_count * sizeof(Elf_Phdr) + sections_count * sizeof(Elf_Shdr);
	off_t note_offset = strtab_offset + sizeof(strtab);
	size_t note_size = core_note_size(&files_note);
	off_t seg_data_offset = note_offset + note_size;


	// write note phdr
	Elf_Phdr note_phdr = {
		.p_type   = PT_NOTE,
		.p_flags  = PF_R,
		.p_offset = note_offset,
		.p_filesz = note_size,
	};
	ret = core_write_exact(fd, &note_phdr, sizeof(note_phdr));
	if (ret < 0) {
free_notes:
		core_free_note(&files_note);
		return ret;
	}

	// write phdrs
	off_t current_offset = seg_data_offset;
	avltree_foreach (node, &space->segs) {
		vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
		long flags = 0;
		if (seg->prot & MMU_FLAG_READ) {
			flags |= PF_R;
		}
		if (seg->prot & MMU_FLAG_WRITE) {
			flags |= PF_W;
		}
		if (seg->prot & MMU_FLAG_EXEC) {
			flags |= PF_X;
		}

		Elf_Phdr phdr = {
			.p_type   = PT_LOAD,
			.p_flags  = flags,
			.p_vaddr  = seg->start,
			.p_paddr  = seg->start,
			.p_offset = current_offset,
			.p_memsz  = VMM_SIZE(seg),
			.p_filesz = VMM_SIZE(seg),
			.p_align  = PAGE_SIZE,
		};
		ret	= core_write_exact(fd, &phdr, sizeof(phdr));
		if (ret < 0) goto free_notes;
		current_offset += VMM_SIZE(seg);
	}

	// write sections
	Elf_Shdr null_section = {
		.sh_type = SHT_NULL,
	};
	ret = core_write_exact(fd, &null_section, sizeof(null_section));
	if (ret < 0) goto free_notes;

	Elf_Shdr note_section = {
		.sh_name   = CORE_NAME_NOTE0,
		.sh_type   = SHT_NOTE,
		.sh_flags  = SHF_ALLOC,
		.sh_offset = note_offset,
		.sh_size   = note_size,
		.sh_addralign = 1,
	};
	ret = core_write_exact(fd, &note_section, sizeof(note_section));
	if (ret < 0) goto free_notes;

	// write segments sections
	current_offset = seg_data_offset;
	avltree_foreach (node, &space->segs) {
		vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
		long flags = SHF_ALLOC;
		if (seg->prot & MMU_FLAG_WRITE) {
			flags |= SHF_WRITE;
		}
		if (seg->prot & MMU_FLAG_EXEC) {
			flags |= SHF_EXECINSTR;
		}

		Elf_Shdr section = {
			.sh_name   = CORE_NAME_LOAD,
			.sh_type   = SHT_PROGBITS,
			.sh_flags  = flags,
			.sh_addr   = seg->start,
			.sh_offset = current_offset,
			.sh_size   = VMM_SIZE(seg),
			.sh_addralign = 1,
		};
		ret	= core_write_exact(fd, &section, sizeof(section));
		if (ret < 0) goto free_notes;
		current_offset += VMM_SIZE(seg);
	}

	Elf_Shdr strtab_section = {
		.sh_name = CORE_NAME_SHSTRTAB,
		.sh_type = SHT_STRTAB,
		.sh_flags = 0,
		.sh_offset = strtab_offset,
		.sh_size = sizeof(strtab),
		.sh_addralign = 1,
	};
	ret = core_write_exact(fd, &strtab_section, sizeof(strtab_section));
	if (ret < 0) goto free_notes;
	
	// write strtab
	ret = core_write_exact(fd, strtab, sizeof(strtab));
	if (ret < 0) goto free_notes;

	// write notes
	ret = core_write_note(fd, &files_note);
	if (ret < 0) goto free_notes;
	core_free_note(&files_note);
	
	// write segment data
	avltree_foreach (node, &space->segs) {
		vmm_seg_t *seg = container_of(node, vmm_seg_t, node);
		ret = core_write_exact(fd, (void*)seg->start, VMM_SIZE(seg));
		if (ret < 0) return ret;
	}

	return 0;
}

int core_write(vfs_fd_t *fd, siginfo_t *siginfo) {
	vmm_space_t *space = vmm_get_current_space();
	rwsem_acquire_read(&space->lock);
	int ret = core_raw_write(fd, siginfo, space);
	rwsem_release_read(&space->lock);
	return ret;
}

int core_dump(siginfo_t *siginfo) {
	// TODO : make the path configurable
	char name[PATH_MAX];
	snprintf(name, sizeof(name), "core.%ld", get_current_proc()->pid);
	vfs_fd_t *fd = vfs_open(name, O_CREAT, 0600);
	if (IS_ERR(fd)) return PTR2ERR(fd);

	int ret = core_write(fd, siginfo);

	vfs_close(fd);
	return ret;
}
