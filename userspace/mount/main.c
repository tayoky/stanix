#include <sys/mount.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void help(){
	printf("mount -t TYPE [-s] SOURCE [-T] TARGET\n");
	printf("-t/--type   : precise type\n");
	printf("-s/--source : precise source/device to mount (can be a stub for tmpfs)\n");
	printf("-T/--target : path to mount to\n");
}

int main(int argc,char **argv){
	char *type = NULL;
	char *source = NULL;
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
		if(!(strcmp(argv[i],"--source") && strcmp(argv[i],"-S"))){
			i++;
			if(!argv[i]){
				fprintf(stderr,"expect path after --source\n");
				return 1;
			}
			source = argv[i];
			continue;
		}
		if(!(strcmp(argv[i],"--type") && strcmp(argv[i],"-t"))){
			i++;
			if(!argv[i]){
				fprintf(stderr,"expect type after --type\n");
				return 1;
			}
			type = argv[i];
			continue;
		}
		if(!strcmp(argv[i],"--help")){
			help();
			return 0;
		}
		if(!source){
			source = argv[i];
			continue;
		}
		if(!target){
			target = argv[i];
			continue;
		}
	}
	
	int ret = 0;
	if(!type){
		fprintf(stderr,"mount : no type specified\n");
		ret = 1;
	}
	if(!source){
		fprintf(stderr,"mount : no source specified\n");
		ret = 1;
	}
	if(!target){
		fprintf(stderr,"mount : no target specified\n");
		ret = 1;
	}

	if(ret){
		return ret;
	}

	printf("mount : mounting the device %s under %s , type : %s\n",source,target,type);

	ret = mount(source,target,type,0,NULL);
	if(ret < 0){
		perror("mount");
	}
	return ret;
}