#include "vfs.h"
#include "print.h"
#include "asm.h"
#include "kheap.h"
#include "kernel.h"
#include "string.h"
#include "ini.h"

void read_main_conf_file(void){
	kstatus("open main conf file initrd:/conf.ini ...");
	vfs_node *conf_file = vfs_open("initrd:/conf.ini");

	//retry with stanix.ini
	if(!conf_file){
		kinfof("fail to open initrd:/conf.ini\n");
		kinfof("try to open initrd:/stanix.ini instead\n");
		conf_file = vfs_open("initrd:/stanix.ini");
	}

	if(!conf_file){
		kinfof("can't open initrd:/stanix.ini\n");
		kfail();
		kinfof("no conf file find\n");
		halt();
	}

	//now read it
	char *buffer = kmalloc(conf_file->size + 1);

	if(vfs_read(conf_file,buffer,0,127) != conf_file->size){
		kfail();
		kdebugf("fail to read from conf file\n");
		halt();
	}
	buffer[conf_file->size] = '\0';

	kernel->conf_file = buffer;

	vfs_close(conf_file);
	kok();
}

static const char *next_line(const char *line){
	while(*line != '\n' && *line != '\0'){
		line++;
	}

	if(*line == '\n'){
		line++;
	}
	return line;
}

static const char *skip_space(const char* str){
	while(*str == ' '){

		//if end return now
		if(*str == '\0'){
			return str;
		}
		str++;
	}
	return str;
}

char *ini_get_value(const char*ini_file,const char *section,const char *key){

	//the header fo the section
	char *section_header = kmalloc(strlen(section) + 3);
	strcpy(section_header,"[");
	strcat(section_header,section);
	strcat(section_header,"]");
	size_t section_header_len = strlen(section_header);

	//now go trough each line and find the header
	const char *current = ini_file;
	while(*current){
		current = skip_space(current);
		if(!memcmp(current,section_header,section_header_len)){
			//we find it !!
			break;
		}
		current = next_line(current);
	}

	if(!*current){
		//we don't find it
		return NULL;
	}

	//go to the next line
	current = next_line(current);
	current = skip_space(current);

	//same thing with the key
	size_t key_len = strlen(key);
	while(*current){
		if(!memcmp(current,key,key_len)){
			//we find it !!
			break;
		}
		current = next_line(current);
		current = skip_space(current);
		if(*current == '['){
			//it's the next section
			//the key don't exist
			break;
		}
	}

	if((!*current) || *current == '['){
		//the key don't exist
		return NULL;
	}

	//we find the key now find his value
	size_t value_len = 0;

	current+= key_len;
	current = skip_space(current);
	if(*current != '='){
		//the key don't have a value
		//return the key name
		char *ret = kmalloc(key_len + 1);
		strcpy(ret,key);
		return ret;
	}
	current++;
	current = skip_space(current);
	while(*current != '\0' && *current != '\n'){
		value_len++;
		current++;
	}
	current-= value_len;

	char *value = kmalloc(value_len + 1);
	memcpy(value,current,value_len);
	value[value_len] = '\0';

	return value;
}