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

vfs_node *new_tarfs(const char *tar_file){
	vfs_node *root = new_tmpfs();
	ustar_header *current_file = (ustar_header *)tar_file;


	while(!memcmp(current_file->ustar,"ustar",5)){
		uint64_t file_size = octal2int(current_file->file_size);
		kdebugf("find file %s , size : %s[octal] %lu[int]\n",current_file->name,current_file->file_size,file_size);
		current_file += ((file_size + 1023) / 512) * 512;
		kprintf("ustar indicator : %c%c%c%c%c\n",current_file->ustar[0],current_file->ustar[1],current_file->ustar[2],current_file->ustar[3],current_file->ustar[4]);
	}
	return NULL;
}

void mount_initrd(void){
	kstatus("unpack initrd ...");
	if(vfs_mount("initrd",new_tarfs(kernel->initrd->address))){
		kfail();
		halt();
	}
	kok();
}