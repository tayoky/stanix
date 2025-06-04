#include <sys/mount.h>
#include <string.h>
#include <stdio.h>

int main(int argc,char **argv){
	char *type = NULL;
	char *source = NULL;
	char *target = NULL;

	for (int i = 1; i < argc - 1; i++){
		if(!(strcmp(argv[i],"--target") && strcmp(argv[i],"-T"))){
			i++;
			target = argv[i];
			continue;
		}
		if(!(strcmp(argv[i],"--source") && strcmp(argv[i],"-S"))){
			i++;
			source = argv[i];
			continue;
		}
		if(!(strcmp(argv[i],"--type") && strcmp(argv[i],"-t"))){
			i++;
			type = argv[i];
			continue;
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

	printf("mount : mounting the device %s under %s , type : %s",source,target,type);

	return mount(source,target,type,0,NULL);
}