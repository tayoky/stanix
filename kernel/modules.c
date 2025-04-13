#include "module.h"
#include "print.h"
#include "vfs.h"
#include <elf.h>
#include "string.h"
#include "paging.h"
#include <errno.h>

//dynamic module loading

extern uint64_t p_kernel_end[];
static void *ptr = (void *)((uintptr_t)&p_kernel_end) + PAGE_SIZE;

static int check_mod_header(Elf64_Ehdr *header){
	if(memcmp(header->e_ident,ELFMAG,4)){
		return 0;
	}

	if(header->e_ident[EI_VERSION] != EV_CURRENT){
		return 0;
	}

	if(header->e_ident[EI_DATA] != ELFDATA2LSB){
		return 0;
	}
	
	if(header->e_ident[EI_CLASS] != ELFCLASS64){
		return 0;
	}

	if(header->e_type != ET_REL){
		return 0;
	}

	return 1;
}

static void *map_mod(size_t size){
	ptr = (void *)PAGE_ALIGN_UP((uintptr_t)ptr);
	void *buf = ptr;
	while(size > PAGE_SIZE){
		map_page(get_addr_space() + kernel->hhdm,allocate_page(&kernel->bitmap),(uintptr_t)ptr/PAGE_SIZE,PAGING_FLAG_RW_CPL0);
		size -= PAGE_SIZE;
		ptr += PAGE_SIZE;
	}
	return buf;
}

static inline Elf64_Shdr *get_Shdr(char *start,Elf64_Ehdr *header,Elf64_Word index){
	return (Elf64_Shdr *)start + header->e_shoff + index * header->e_shentsize;
}

int insmod(const char *pathname,const char **args,char **name){
	int ret = -ENOSYS;
	kdebugf("try to insmod %s\n",pathname);
	vfs_node *file = vfs_open(pathname,VFS_READONLY);
	if(!file){
		return -ENOENT;
	}

	Elf64_Ehdr header;
	vfs_read(file,&header,0,sizeof(Elf64_Ehdr));

	if(!check_mod_header(&header)){
		kdebugf("invalid ELF header\n");
		ret = -ENOEXEC;
		goto close;
	}

	//load the entire things into memory
	char *mod = map_mod(PAGE_ALIGN_UP(file->size));
	vfs_read(file,mod,0,file->size);

	//update sections address
	for(int i=0; i<header.e_shnum; i++){
		Elf64_Shdr *sheader = get_Shdr(mod,&header,i);
		sheader->sh_addr = mod + sheader->sh_offset;
	}

	close:
	vfs_close(file);
	return ret;
}

int rmmod(const char *name){
	return -ENOSYS;
}