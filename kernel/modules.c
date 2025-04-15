#include <kernel/module.h>
#include "print.h"
#include <kernel/vfs.h>
#include <elf.h>
#include "string.h"
#include <kernel/paging.h>
#include "sym.h"
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
		map_page((uint64_t *)(get_addr_space() + kernel->hhdm),allocate_page(&kernel->bitmap),(uintptr_t)ptr/PAGE_SIZE,PAGING_FLAG_RW_CPL0);
		size -= PAGE_SIZE;
		ptr += PAGE_SIZE;
	}
	return buf;
}

uintptr_t sym_lookup(const char *name){
	for (size_t i = 0; i < symbols_count; i++){
		if(!strcmp(symbols[i].name,name)){
			return symbols[i].value;
		}
	}
	return 0;
}

#define get_Shdr(index) \
((Elf64_Shdr *)((uintptr_t)mod + header.e_shoff + (index * header.e_shentsize)))

int insmod(const char *pathname,const char **args,char **name){
	(void)args;
	(void)name;

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
		sheader->sh_addr =(Elf64_Addr)mod + sheader->sh_offset;
	}

	kmodule *module_meta = NULL;

	//relocate and link symbols
	for(int i=0; i<header.e_shnum; i++){
		Elf64_Shdr *sheader = get_Shdr(i);
		if(sheader->sh_type != SHT_SYMTAB) continue;

		//get the symbols table and the string table
		Elf64_Sym *symtab = (Elf64_Sym *)sheader->sh_addr;
		char *strtab = (char *)get_Shdr(sheader->sh_link)->sh_addr;
		kdebugf("sym tab : %d\n",sheader->sh_link);

		//iterate trought each symbol
		//note that we skip the first symbol ( the NULL symbol)
		for (size_t i = 1; i < sheader->sh_size / sizeof(Elf64_Sym); i++){
			if(symtab[i].st_shndx > SHN_UNDEF && symtab[i].st_shndx < SHN_LOPROC){
				//symbol to relocate
				kdebugf("relocate\n");
				symtab[i].st_value += get_Shdr(symtab[i].st_shndx)->sh_addr;
			} else if (symtab[i].st_shndx == SHN_UNDEF){
				//symbols undefined (maybee we can link it)
				uintptr_t value = sym_lookup(strtab + symtab[i].st_name);
				if(value){
					kdebugf("link sym :\n");
					symtab[i].st_value += value;
				} else {
					kdebugf("insmod : undefined symbol %s\n",strtab + symtab[i].st_name);
					goto unmap;
				}
			}

			if(!strcmp(strtab + symtab[i].st_name,"module_meta")){
				module_meta = (kmodule *)symtab[i].st_value;
			}
			kdebugf("sym %s : %p\n",strtab + symtab[i].st_name,symtab[i].st_value);
		}
		
	}

	//check it as meta
	if(!module_meta){
		kdebugf("insmod : module don't contain metadata\n");
		goto unmap;
	}

	//apply relocation
	for(int i=0; i<header.e_shnum; i++){
		Elf64_Shdr *sheader = get_Shdr(i);
		if(sheader->sh_type != SHT_RELA) continue;

		//find the relocation table and the symbol table and the section it apply to
		Elf64_Rela *rela = (Elf64_Rela *)sheader->sh_addr;
		Elf64_Sym *symtab = (Elf64_Sym *)get_Shdr(sheader->sh_link)->sh_addr;
		Elf64_Shdr *section = get_Shdr(sheader->sh_info);

		//ietarate trought each relocation
		for (size_t i = 0; i < sheader->sh_size / sizeof(Elf64_Rela); i++){

			//let define some macro
			kdebugf("apply relocation off : %lx\n",rela[i].r_offset);
			#define A rela[i].r_addend
			#define B mod
			#define P (section->sh_addr + rela[i].r_offset)
			#define S symtab[ELF64_R_SYM(rela[i].r_info)].st_value
			#define Z symtab[ELF64_R_SYM(rela[i].r_info)].st_size

			uint32_t w32;
			uint64_t w64;

			switch(ELF64_R_TYPE(rela[i].r_info)){
#ifdef x86_64
			case R_X86_64_NONE:
				break;
			case R_X86_64_64:
				w64 = S + A;
				memcpy((void *)P,&w64,sizeof(uint64_t));
				break;
			case R_X86_64_PC32:
				w32 = S + A - P;
				memcpy((void *)P,&w32,sizeof(uint32_t));
				break;
			case R_X86_64_32:
				w32 = S + A;
				memcpy((void *)P,&w32,sizeof(uint32_t));
				break;
#endif
			default :
				kdebugf("unknow relocation type %d\n",ELF64_R_TYPE(rela[i].r_info));
			}
		}
		

	}

	kdebugf("module name : %s\n",module_meta->name);

	//count argc and launch init function
	int argc = 0;
	while(args[argc]){
		argc++;
	}
	kdebugf("init func : 0x%p\n",module_meta->init);
	ret = module_meta->init(argc,(char **)args);
	kdebugf("return status : %d\n",ret);

	close:
	vfs_close(file);
	return ret;

	unmap:
	//TODO actually unmap
	kdebugf("insmod failed\n");
	goto close;
}

int rmmod(const char *name){
	(void)name;
	return -ENOSYS;
}

void init_mod(){
	kstatus("init modules loaded ... ");

	kok();
}

void __export_symbol(void *sym,const char *name){
	kdebugf("try export %s\n",name);
}
void __unexport_symbol(void *sym,const char *name){

}