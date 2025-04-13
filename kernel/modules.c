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
	kdebugf("try to map module at 0x%p size : %ld\n",ptr,size);
	while(size >= PAGE_SIZE){
		map_page(get_addr_space() + kernel->hhdm,allocate_page(&kernel->bitmap),(uintptr_t)ptr/PAGE_SIZE,PAGING_FLAG_RW_CPL0);
		size -= PAGE_SIZE;
		ptr += PAGE_SIZE;
	}
	return buf;
}

#define get_Shdr(index) \
((Elf64_Shdr *)((uintptr_t)mod + header.e_shoff + (index * header.e_shentsize)))

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
		Elf64_Shdr *sheader = get_Shdr(i);
		sheader->sh_addr = mod + sheader->sh_offset;
	}

	//symbols stuff
	for(int i=0; i<header.e_shnum; i++){
		Elf64_Shdr *sheader = get_Shdr(i);
		if(sheader->sh_type != SHT_SYMTAB) continue;

		Elf64_Sym *symbols = sheader->sh_addr;
		char *strtab = get_Shdr(sheader->sh_link)->sh_addr;
		kdebugf("sym tab : %d\n",sheader->sh_link);
		for (size_t i = 0; i < sheader->sh_size / sizeof(Elf64_Sym); i++){
			if(symbols[i].st_shndx > 0 && symbols[i].st_shndx < 0xff00){
				kdebugf("relocate\n");
				symbols[i].st_value += get_Shdr(symbols[i].st_shndx)->sh_addr;
			}
			kdebugf("sym %s : %p\n",strtab + symbols[i].st_name,symbols[i].st_value);
		}
		
	}

	close:
	vfs_close(file);
	return ret;
}

int rmmod(const char *name){
	return -ENOSYS;
}

void init_mod(){
	kstatus("init modules loaded ... ");

	kok();
}