#include <kernel/kout.h>
#include <kernel/string.h>
#include <kernel/kheap.h>
#include <kernel/vfs.h>
#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/ini.h>

void init_kout(){
	//read the conf file,
	//open all the scpefied file/dev
	//and save it!
	//simple right ?

	kstatusf("init kout... ");

	//first find the number of out
	int kout_count = 0;
	
	for(;;){
		char current_key[64];
		sprintf(current_key,"kout%d",kout_count);
		char *current_kout = ini_get_value(kernel->conf_file,"kout",current_key);
		if(!current_kout){
			break;
		}
		kfree(current_kout);
		kout_count++;
	}

	if(!kout_count){
		kfail();
		kinfof("can't find any kout all kernel output will still be via serial\n");
		return;
	}

	//now open and make the list
	vfs_fd_t **outs = kmalloc(sizeof(vfs_fd_t *) * (kout_count + 1));
	outs[kout_count] = NULL;

	for (int index = 0; index < kout_count; index++){
		char current_key[64];
		sprintf(current_key,"kout%d",index);
		char *current_kout = ini_get_value(kernel->conf_file,"kout",current_key);

		vfs_fd_t *current_fd = vfs_open(current_kout,O_WRONLY);
		if(!current_fd){
			kinfof("can't open %s \n",current_kout);
			kfree(current_kout);
			continue;
		}
		outs[index] = current_fd;
		kfree(current_kout);
	}
	
	//now actually use it
	kernel->outs = outs;

	kok();
	kinfof("find %ld kouts\n",kout_count);
}