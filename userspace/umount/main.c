#include <sys/mount.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void help(){
	printf("umount TARGET\n");
}

int main(int argc,char **argv){
	char *target = NULL;

	for (int i = 1; i < argc; i++){
		if(!(strcmp(argv[i],"--target") && strcmp(argv[i],"-T"))){
			i++;
			if(!argv[i]){
				fprintf(stderr,"expect path after --target\n");
				return 1;
			}
			target = argv[i];
			continue;
		}
		if(!target){
			target = argv[i];
			continue;
		}
	}
	
	int ret = 0;
	if(!target){
		fprintf(stderr,"umount : no target specified\n");
		ret = 1;
	}

	if(ret){
		return ret;
	}

	ret = umount(target);
	if(ret < 0){
		perror("umount");
		return 1;
	}
	return 0;
}