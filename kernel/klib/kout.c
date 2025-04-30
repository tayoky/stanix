#include <kernel/kout.h>
#include <kernel/string.h>
#include <kernel/kheap.h>
#include <kernel/vfs.h>
#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/ini.h>

char *uint2str(uintmax_t integer){
	uint16_t digits = 1;

	//reprsent the maxmium number that can be represented +1
	uintmax_t digit_max = 10;
	while(integer >= digit_max ){
		digits++;
		digit_max *= 10;
	}

	//we have found the number of digit now allocate momory for the string
	char *str = kmalloc(digits + 1);
	str[digits] = '\0';

	while(digits){
		digits--;
		str[digits] = '0' + (integer % 10);
		integer /= 10;
	}

	return str;
}

static char * key_name_from_int(uintmax_t index){
	char *num = uint2str(index);
	char *str = kmalloc(strlen("kout") + strlen(num) + 1);
	strcpy(str,"kout");
	strcat(str,num);
	kfree(num);
	return str;
}

void init_kout(){
	//read the conf file,
	//open all the scpefied file/dev
	//and save it!
	//simple right ?

	kstatus("init kout... ");

	//first find the number of out
	uintmax_t kout_count = 0;
	
	for(;;){
		char *current_key = key_name_from_int(kout_count);
		char *current_kout = ini_get_value(kernel->conf_file,"kout",current_key);
		kfree(current_key);
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

	for (size_t index = 0; index < kout_count; index++){
		char *current_key = key_name_from_int(index);
		char *current_kout = ini_get_value(kernel->conf_file,"kout",current_key);

		kfree(current_key);
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