#include "module.h"
#include "print.h"
#include "vfs.h"
#include <errno.h>

//dynamic module loading


int insmod(const char *pathname,const char **args,char **name){
	return -ENOSYS;
}

int rmmod(const char *name){
	return -ENOSYS;
}