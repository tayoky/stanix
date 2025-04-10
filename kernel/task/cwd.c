#include "cwd.h"
#include "string.h"
#include "kheap.h"
#include "scheduler.h"

char *absolute_path(const char *path){
	//if path start with / it's aready absolute
	if(path[0] == '/'){
		return strdup(path);
	}

	char *new_path = kmalloc(strlen(get_current_proc()->cwd_path) + strlen(path) + 1);
	strcpy(new_path,get_current_proc()->cwd_path);
	strcat(new_path,path);
	return new_path;
}