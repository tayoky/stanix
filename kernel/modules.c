#include <kernel/module.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/vfs.h>
#include <kernel/string.h>
#include <kernel/paging.h>
#include <kernel/sym.h>
#include <errno.h>
#include <elf.h>

//dynamic module loading

extern uintptr_t p_kernel_end[];
static uintptr_t ptr = ((uintptr_t)&p_kernel_end) + PAGE_SIZE;

#ifdef i386
#define Elf_Ehdr Elf32_Ehdr
#define Elf_Shdr Elf32_Shdr
#define Elf_Addr Elf32_Addr
#define Elf_Sym  Elf32_Sym
#define Elf_Rela Elf32_Rela
#define ELF_R_SYM(i)  ELF32_R_SYM(i)
#define ELF_R_TYPE(i) ELF32_R_TYPE(i)
#define ELF_R_INFO(i) ELF32_R_INFO(i)
#else
#define Elf_Ehdr Elf64_Ehdr
#define Elf_Shdr Elf64_Shdr
#define Elf_Addr Elf64_Addr
#define Elf_Sym  Elf64_Sym
#define Elf_Rela Elf64_Rela
#define ELF_R_SYM(i)  ELF64_R_SYM(i)
#define ELF_R_TYPE(i) ELF64_R_TYPE(i)
#define ELF_R_INFO(i) ELF64_R_INFO(i)
#endif


exported_sym *exported_sym_list = NULL;
list *loaded_mods;

int check_mod_header(Elf_Ehdr *header){
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

void *map_mod(size_t size){
	size = PAGE_ALIGN_UP(size);
	ptr = PAGE_ALIGN_UP(ptr);
	void *buf = (void *)ptr;
	kdebugf("insmod : map module at 0x%p size : %ld\n",ptr,size);
	while(size > 0){
		map_page(get_addr_space(),pmm_allocate_page(),ptr,PAGING_FLAG_RW_CPL0);
		size -= PAGE_SIZE;
		ptr += PAGE_SIZE;
	}
	return buf;
}

uintptr_t sym_lookup(const char *name){
	exported_sym *current = exported_sym_list;
	while(current){
		if(!strcmp(name,current->name)){
			return current->value;
		}
		current = current->next;
	}
	return 0;
}

#define get_Shdr(index) \
((Elf_Shdr *)((uintptr_t)mod + header.e_shoff + (index * header.e_shentsize)))

int insmod(const char *pathname,const char **args,char **name){
	(void)args;
	(void)name;

	int ret = -ENOSYS;
	kdebugf("try to insmod %s\n",pathname);
	vfs_node *file = vfs_open(pathname,VFS_READONLY);
	if(!file){
		return -ENOENT;
	}

	Elf_Ehdr header;
	vfs_read(file,&header,0,sizeof(Elf_Ehdr));

	if(!check_mod_header(&header)){
		kdebugf("insmod : invalid ELF header\n");
		ret = -ENOEXEC;
		goto close;
	}

	//load the entire things into memory
	struct stat st;
	vfs_getattr(file,&st);
	char *mod = map_mod(PAGE_ALIGN_UP(st.st_size));
	vfs_read(file,mod,0,st.st_size);

	loaded_module *module = kmalloc(sizeof(loaded_module));
	memset(module,0,sizeof(loaded_module));
	kmodule_section *main_section = kmalloc(sizeof(kmodule_section));
	memset(main_section,0,sizeof(kmodule_section));

	module->sections = new_list();
	list_append(module->sections,main_section);
	main_section->base = mod;
	main_section->size = PAGE_ALIGN_UP(st.st_size);

	//update sections address and allocate no bits
	for(int i=0; i<header.e_shnum; i++){
		Elf_Shdr *sheader = get_Shdr(i);
		if(sheader->sh_type == SHT_NOBITS && sheader->sh_flags & SHF_ALLOC){
			kmodule_section *section = kmalloc(sizeof(kmodule_section));
			memset(section,0,sizeof(kmodule_section));
			section->base = map_mod(sheader->sh_size);
			section->size = sheader->sh_size;
			memset(section->base,0,section->size);
			sheader->sh_addr = (Elf_Addr)section->base;
			list_append(module->sections,section);
		} else {
			sheader->sh_addr = (Elf_Addr)mod + sheader->sh_offset;
		}
	}

	kmodule *module_meta = NULL;

	//relocate and link symbols
	for(int i=0; i<header.e_shnum; i++){
		Elf_Shdr *sheader = get_Shdr(i);
		if(sheader->sh_type != SHT_SYMTAB) continue;

		//get the symbols table and the string table
		Elf_Sym *symtab = (Elf_Sym *)sheader->sh_addr;
		char *strtab = (char *)get_Shdr(sheader->sh_link)->sh_addr;

		//iterate trought each symbol
		//note that we skip the first symbol ( the NULL symbol)
		for (size_t i = 1; i < sheader->sh_size / sizeof(Elf_Sym); i++){
			if(symtab[i].st_shndx > SHN_UNDEF && symtab[i].st_shndx < SHN_LOPROC){
				//symbol to relocate
				symtab[i].st_value += get_Shdr(symtab[i].st_shndx)->sh_addr;
			} else if (symtab[i].st_shndx == SHN_UNDEF){
				//symbols undefined (maybee we can link it)
				uintptr_t value = sym_lookup(strtab + symtab[i].st_name);
				if(value){
					symtab[i].st_value += value;
				} else {
					kdebugf("insmod : undefined symbol %s\n",strtab + symtab[i].st_name);
					ret = -ELIBACC;
					goto unmap;
				}
			}

			if(!strcmp(strtab + symtab[i].st_name,"module_meta")){
				module_meta = (kmodule *)symtab[i].st_value;
			}
			//kdebugf("sym %s : %p\n",strtab + symtab[i].st_name,symtab[i].st_value);
		}
		
	}

	//check it as meta
	if(!module_meta){
		kdebugf("insmod : module don't contain metadata\n");
		ret = -ENOEXEC;
		goto unmap;
	}

	//update the base address in the metadata struct
	module_meta->base = mod;

	//apply relocation
	for(int i=0; i<header.e_shnum; i++){
		Elf_Shdr *sheader = get_Shdr(i);
		if(sheader->sh_type != SHT_RELA) continue;

		//find the relocation table and the symbol table and the section it apply to
		Elf_Rela *rela = (Elf_Rela *)sheader->sh_addr;
		Elf_Sym *symtab = (Elf_Sym *)get_Shdr(sheader->sh_link)->sh_addr;
		Elf_Shdr *section = get_Shdr(sheader->sh_info);

		//ietarate trought each relocation
		for (size_t i = 0; i < sheader->sh_size / sizeof(Elf_Rela); i++){

			//let define some macro
			#define A rela[i].r_addend
			#define B mod
			#define P (section->sh_addr + rela[i].r_offset)
			#define S symtab[ELF_R_SYM(rela[i].r_info)].st_value
			#define Z symtab[ELF_R_SYM(rela[i].r_info)].st_size

#ifndef __i386__
			uint32_t w32;
			uint64_t w64;
#endif

			switch(ELF_R_TYPE(rela[i].r_info)){
#ifdef __x86_64__
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
				kdebugf("insmod : unknow relocation type %d\n",ELF_R_TYPE(rela[i].r_info));
				ret = -ENOEXEC;
				goto unmap;
			}
		}
		

	}

	kdebugf("insmod : module name : %s\n",module_meta->name);

	//count argc and launch init function
	int argc = 0;
	while(args[argc]){
		argc++;
	}
	ret = module_meta->init(argc,(char **)args);
	kdebugf("return status : %d\n",ret);

	//if return an error code then unmap
	if(ret < 0){
		goto unmap;
	}

	//add the module to the list
	module->meta = module_meta;
	list_append(loaded_mods,module);

	close:
	vfs_close(file);
	return ret;

	unmap:
	//TODO actually unmap
	kdebugf("insmod : insmod failed\n");
	goto close;
}

int rmmod(const char *name){
	(void)name;
	return -ENOSYS;
}

void init_mod(){
	kstatus("init exported symbol list and module loader ... ");

	//init the list to keep track of all modules
	loaded_mods = new_list();

	//export all symbols of the core module
	for (size_t i = 0; i < symbols_count; i++){
		__export_symbol((void *)symbols[i].value,symbols[i].name);
	}

	kok();
}

void __export_symbol(void *sym,const char *name){
	//add the symbol at the start of the list
	exported_sym *new_sym = kmalloc(sizeof(exported_sym));
	new_sym->value = (uintptr_t)sym;
	new_sym->name = name;
	new_sym->next = exported_sym_list;
	exported_sym_list = new_sym;
}

void __unexport_symbol(void *sym,const char *name){
	//search the sym in the list and remove it
	exported_sym *current_sym = exported_sym_list;
	exported_sym *prev = NULL;
	while(current_sym){
		if((!strcmp(current_sym->name,name) && (uintptr_t)sym == current_sym->value)){
			if(prev){
				prev->next = current_sym->next;
			} else {
				exported_sym_list = current_sym->next;
			}
			kfree(current_sym);
			return;
		}
		prev = current_sym;
		current_sym = current_sym->next;
	}
}

//a simple tool used by modules
int have_opt(int argc,char **argv,char *option){
	for (int i = 0; i < argc; i++){
		if(!strcmp(argv[i],option)){
			return 1;
		}
	}
	return 0;
}