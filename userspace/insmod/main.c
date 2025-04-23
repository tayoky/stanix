#include <stdio.h>
#include <string.h>
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

	if(insmod(argv[1],&argv[1])){
		perror("insmod");
		return 1;
	}
	return 0;
}
