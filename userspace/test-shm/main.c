#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

int main(int argc,char **argv){
	if(argc <2 && argc > 3){
		fprintf(stderr,"usage : test-shm PATH [MSG]\n");
		return 1;
	}

	if(argc == 2){
		//recive
		int fd = shm_open(argv[1],O_RDONLY,0);
		if(fd < 0)perror(argv[1]);
		struct stat st;
		fstat(fd,&st);
		void *mapping = mmap(NULL,st.st_size,PROT_READ,MAP_SHARED,fd,0);
		printf("%s\n",(char*)mapping);
		shm_unlink(argv[1]);
		close(fd);
	} else {
		//send
		int fd = shm_open(argv[1],O_WRONLY | O_TRUNC | O_CREAT,0777);
		ftruncate(fd,strlen(argv[2])+1);
		void *mapping = mmap(NULL,strlen(argv[2])+1,PROT_READ | PROT_WRITE,MAP_SHARED,fd,0);
		strcpy(mapping,argv[2]);
		close(fd);
	}
	return 0;
}