#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>

void help(void){
	puts("info DEVICE");
	puts("show information about a device");
}

int main(int argc,char **argv){
	if(argc != 2){
		fprintf(stderr,"not enought argument\n");
		return 1;
	}
	if(!strcmp(argv[1],"--help")){
		help();
		return 0;
	}
	int fd = open(argv[1],O_RDONLY);
	if(fd < 0){
		perror(argv[1]);
		return 1;
	}
	char model[256];
	if(ioctl(fd,I_MODEL,model) < 0){
		perror("ioctl");
		return 1;
	}
	printf("%s :\n",argv[1]);
	printf("model : %s\n",model);
	return 0;
}
