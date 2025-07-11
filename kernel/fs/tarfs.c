#include <kernel/tarfs.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <kernel/tmpfs.h>
#include <kernel/print.h>
#include <kernel/asm.h>

static inline uint64_t octal2int(const char *octal){
	uint64_t integer = 0;
	while(*octal){
		integer *= 8;
		integer += (*octal) - '0';
		octal++;
	}
	return integer;
}

#define CHECK(o,m) if(i & o)mode |= m

static mode_t octal2mode(const char *octal){
	int i = octal2int(octal);

	mode_t mode = 0;

	CHECK(TOEXEC ,0x1);
	CHECK(TOWRITE,0x2);
	CHECK(TOREAD ,0x4);
	CHECK(TGEXEC ,0x10);
	CHECK(TGWRITE,0x20);
	CHECK(TGREAD ,0x40);
	CHECK(TSGID,  0x80);
	CHECK(TUEXEC ,0x100);
	CHECK(TUWRITE,0x200);
	CHECK(TUREAD ,0x400);
	CHECK(TSUID,  0x800);

	return mode;
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
		char full_path[strlen(current_file->name) + strlen("/") + 1];
		sprintf(full_path,"/%s",current_file->name);

		//find file size
		size_t file_size = octal2int(current_file->file_size);

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
			vfs_node *file = vfs_open(full_path,VFS_WRITEONLY);
			if(!file){
				kfail();
				kinfof("fail to open file %s\n",full_path);
				halt();
			}

			//set the owner to root
			vfs_chown(file,0,0);

			//set mode
			vfs_chmod(file,octal2mode(current_file->file_mode));

			//copy the files content
			ssize_t write_size = vfs_write(file,addr + 512,0,file_size);
			if((size_t)write_size != file_size){
				kfail();
				kinfof("%s\n",current_file->name);
				kinfof("fail to write to file %s, can only write %luKB/%luKB\n",full_path,write_size/1024,file_size/1024);
				halt();
			}

			//now close and free
			vfs_close(file);
		};

		addr += (((uint64_t)file_size + 1023) / 512) * 512;
	}
	uintptr_t start =  PAGE_ALIGN_UP((uintptr_t)kernel->initrd->address);
	uintptr_t end = PAGE_ALIGN_DOWN((uintptr_t)kernel->initrd->address + kernel->initrd->size);

	//when we page align it might become empty
	while(start < end){
		pmm_free_page((uintptr_t)virt2phys((void *)start));
		start += PAGE_SIZE;
	}
	
	//now free the tar archive
	kok();
}