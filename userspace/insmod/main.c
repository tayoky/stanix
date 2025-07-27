#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/module.h>

int main(int argc,const char **argv){
	if(argc < 2){
		printf("insmod : not enought arguments\n");
		return 1;
	}
	if(!strcmp("--help",argv[1])){
		printf("usage : insmod MODULE [ARGUMENTS]\n");
		return 0;
	}

	if(!strchr(argv[1],'/')){
		char *path = malloc(strlen(argv[1]) + strlen("/mod/") + 1);
		sprintf(path,"/mod/%s",argv[1]);
		argv[1] = path;
	}

	if(insmod(argv[1],&argv[1])){
		perror("insmod");
		return 1;
	}
	return 0;
}
