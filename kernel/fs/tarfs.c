#include "tarfs.h"
#include "kernel.h"
#include "string.h"
#include "tmpfs.h"
#include "print.h"
#include "asm.h"

static uint64_t octal2int(const char *octal){
	uint64_t integer = 0;
	while(*octal){
		integer *= 8;
		integer += (*octal) - '0';
		octal++;
	}
	return integer;
}

static void ls(const char *path){
	vfs_node *node = vfs_open(path);

	struct dirent *ret;
	uint64_t index = 0;
	while(1){
		ret = vfs_readdir(node,index);
		if(!ret)break;
		kprintf("%s\n",ret->d_name);
		index++;
	}

	vfs_close(node);
}

void mount_initrd(void){
	kstatus("unpack initrd ...");
	if(vfs_mount("initrd",new_tmpfs())){
		kfail();
		halt();
	}

	vfs_node *initrd_root = vfs_open("initrd:/");

	char *addr = (char *)kernel->initrd->address;

	while(!memcmp(((ustar_header *)addr)->ustar,"ustar",5)){
		ustar_header *current_file = (ustar_header *)addr;

		//get the path for the parent
		char *parent_path = kmalloc(strlen(current_file->name) + strlen("initrd:/") +1);
		strcpy(parent_path,"initrd:/");
		strcat(parent_path,current_file->name);
		char *i = (char *)((uint64_t)parent_path + strlen(parent_path)-2);
		while ( *i != '/')i--;
		*i = '\0';
		i++;
		kdebugf("parent path : %s\n",parent_path);
		vfs_node *parent = vfs_open(parent_path);

		//it is a folder if it finish with /
		if(current_file->name[strlen(current_file->name) -1] == '/'){
			vfs_mkdir(parent,i,777);
		} else {
			vfs_create(parent,i,777);
		}

		vfs_close(parent);

		uint64_t file_size = octal2int(current_file->file_size);
		addr += (((uint64_t)file_size + 1023) / 512) * 512;
	}

	vfs_close(initrd_root);
	ls("initrd:/");
	kok();
}