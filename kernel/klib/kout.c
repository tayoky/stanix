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

	kstatus("init kout... ");

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
	vfs_node **outs = kmalloc(sizeof(vfs_node *) * (kout_count + 1));
	outs[kout_count] = NULL;

	for (int index = 0; index < kout_count; index++){
		char current_key[64];
		sprintf(current_key,"kout%d",index);
		char *current_kout = ini_get_value(kernel->conf_file,"kout",current_key);

		vfs_node *current_node = vfs_open(current_kout,VFS_WRITEONLY);
		if(!current_node){
			kinfof("can't open %s \n",current_kout);
			kfree(current_kout);
			continue;
		}
		outs[index] = current_node;
		kfree(current_kout);
	}
	
	//now actually use it
	kernel->outs = outs;

	kok();
	kinfof("find %ld kouts\n",kout_count);
}