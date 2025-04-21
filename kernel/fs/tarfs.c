#include <kernel/tarfs.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <kernel/tmpfs.h>
#include <kernel/print.h>
#include <kernel/asm.h>

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
	vfs_node *node = vfs_open(path,VFS_READONLY);

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
	if(vfs_chroot(new_tmpfs())){
		kfail();
		halt();
	}

	char *addr = (char *)kernel->initrd->address;

	//for each file in the tar file create one on the tmpfs
	while(!memcmp(((ustar_header *)addr)->ustar,"ustar",5)){
		ustar_header *current_file = (ustar_header *)addr;

		//find the full path of the file
		char *full_path = kmalloc(strlen(current_file->name) + strlen("/") + 1);
		strcpy(full_path,"/");
		strcat(full_path,current_file->name);

		//find file size
		uint64_t file_size = octal2int(current_file->file_size);

		if(current_file->type == USTAR_DIRTYPE){
			full_path[strlen(full_path)-1] = '\0';

			//create the directory
			if(vfs_mkdir(full_path,0x777)<0){
				kfail();
				kinfof("can't create folder %s for initrd\n",full_path);
				halt();
			}
		} else if(current_file->type == USTAR_REGTYPE){
			//create the file
			if(vfs_create(full_path,0x777,VFS_FILE)){
				kfail();
				kinfof("fail to create file %s\n",full_path);
				halt();
			}

			//open the file
			kdebugf("%s\n",full_path);
			vfs_node *file = vfs_open(full_path,VFS_WRITEONLY);
			if(!file){
				kfail();
				kinfof("fail to open file %s\n",full_path);
				halt();
			}

			//set the owner to root
			vfs_chown(file,0,0);

			//copy the files content
			int64_t write_size = vfs_write(file,addr + 512,0,file_size);
			if((uint64_t)write_size != file_size){
				kfail();
				kinfof("%s\n",current_file->name);
				kinfof("fail to write to file %s, can only write %luKB/%luKB\n",full_path,write_size/1024,file_size/1024);
				halt();
			}

			//now close and free
			vfs_close(file);
		};
		kfree(full_path);

		addr += (((uint64_t)file_size + 1023) / 512) * 512;
	}
	kok();
	ls("/");
}