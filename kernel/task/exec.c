#include "exec.h"
#include "scheduler.h"
#include "kernel.h"
#include "elf.h"
#include "string.h"
#include "paging.h"
#include "print.h"

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

void auto_map(uint64_t *PMLT4,uint64_t address,uint64_t size,uint64_t flags){
	//first convert all of that intp pages
	address = PAGE_ALIGN_DOWN(address) / PAGE_SIZE;
	size = PAGE_ALIGN_UP(size) / PAGE_SIZE;

	while(size > 0){
		map_page(PMLT4,allocate_page(&kernel->bitmap),address,flags);
		address++;
		size--;
	}
}

int exec(char *path){
	vfs_node *file = vfs_open(path);
	if(!file){
		return -1;
	}

	//first read the header
	Elf64_Ehdr header;
	if(vfs_read(file,&header,0,sizeof(header)) < 0){
		error:
		vfs_close(file);
		return -1;
	}

	//then verify it
	if(!verfiy_elf(&header)){
		goto error;
	}
	if(header.e_type != ET_EXEC){
		goto error;
	}

	//now read all program headers
	Elf64_Phdr *prog_header = kmalloc(header.e_phentsize * header.e_phnum);
	if(vfs_read(file,prog_header,header.e_phoff,header.e_phentsize * header.e_phnum) < 0){
		goto error;
	}

	//get the PMLT4
	uint64_t *PMLT4 = get_current_proc()->cr3 + kernel->hhdm;

	//then iterate trought each prog header
	for (size_t i = 0; i < header.e_phnum; i++){
		//only load porgram header with PT_LOAD
		kdebugf("load seg v : 0x%lu\n",prog_header[i].p_memsz);
		if(prog_header[i].p_type != PT_LOAD){
			continue;
		}

		//convert elf header to paging header
		uint64_t flags = PAGING_FLAG_READONLY_CPL3;
		if(prog_header[i].p_flags & PF_X){
			if(prog_header[i].p_flags & PF_R){
				flags |= PAGING_FLAG_RW_CPL3;
			}
		} else{
			flags |= PAGING_FLAG_NO_EXE;
			if(prog_header[i].p_flags & PF_W){
				flags |= PAGING_FLAG_RW_CPL3;
			}
		}
		
		auto_map(PMLT4,prog_header[i].p_vaddr,prog_header[i].p_memsz,flags);
		memset((void*)prog_header[i].p_vaddr,0,prog_header[i].p_memsz);

		//file size must be <= to virtual size
		if(prog_header[i].p_filesz > prog_header[i].p_memsz){
			prog_header[i].p_filesz = prog_header[i].p_memsz;
		}

		if(vfs_read(file,(void*)prog_header[i].p_vaddr,prog_header[i].p_offset,prog_header[i].p_filesz) < 0){
			goto error;
		}

		
	}
	

	vfs_close(file);

	return 0;
}