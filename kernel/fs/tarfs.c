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
	//create an tmpfs for it
	if(vfs_mount("initrd",new_tmpfs())){
		kfail();
		halt();
	}

	char *addr = (char *)kernel->initrd->address;

	//for each file in the tar file create one on the tmpfs
	while(!memcmp(((ustar_header *)addr)->ustar,"ustar",5)){
		ustar_header *current_file = (ustar_header *)addr;

		//get the path for the parent
		char *parent_path = kmalloc(strlen(current_file->name) + strlen("initrd:/") +1);
		strcpy(parent_path,"initrd:/");
		strcat(parent_path,current_file->name);
		char *child = (char *)((uint64_t)parent_path + strlen(parent_path)-2);
		while ( *child != '/')child--;
		*child = '\0';
		child++;
		kdebugf("parent path : %s\n",parent_path);
		vfs_node *parent = vfs_open(parent_path);

		uint64_t file_size = octal2int(current_file->file_size);

		if(!parent){
			kfail();
			kinfof("can't open %s\n",parent_path);
			halt();
		}

		if(current_file->type == USTAR_DIRTYPE){
			child[strlen(child)-1] = '\0';
			if(vfs_mkdir(parent,child,777)<0){
				kfail();
				kinfof("can't create folder %s on %s for initrd\n",child,parent_path);
				halt();
			}
		} else {
			vfs_create(parent,child,777);
			char *full_path = kmalloc(strlen(current_file->name) + strlen("initrd:/") + 1);
			strcpy(full_path,"initrd:/");
			strcat(full_path,current_file->name);
			vfs_node *file = vfs_open(full_path);
			if(!file){
				kfail();
				kinfof("fail to open file : %s\n",full_path);
				halt();
			}
			vfs_write(file,addr + 512,0,file_size);
			//page fault when closing need to fix that
			//vfs_close(file);
		}

		vfs_close(parent);

		addr += (((uint64_t)file_size + 1023) / 512) * 512;
	}

	ls("initrd:/");
	kok();
}