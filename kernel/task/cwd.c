#include "cwd.h"
#include "string.h"
#include "kheap.h"
#include "scheduler.h"

char *absolute_path(const char *path){
	//if a path start with / it's an unix path
	if(path[0] == '/'){
		//emulate unix path with the drive unix:/
		char *abs_path = kmalloc(strlen("unix:") + strlen(path) + 1);
		strcpy(abs_path,"unix:");
		strcat(abs_path,path);
		return abs_path;
	}

	//is it absolute ?
	for (size_t i = 0; path[i]; i++){
		if(path[i] == '/'){
			break;
		}

		if(path[i] == ':'){
			//it is aready an absolute
			return strdup(path);
		}
	}
	
	char *abs_path = kmalloc(strlen(get_current_proc()->cwd_path) + strlen(path) + 1);
	strcpy(abs_path,get_current_proc()->cwd_path);
	strcat(abs_path,path);
	return abs_path;
}