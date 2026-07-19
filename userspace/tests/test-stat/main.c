#include <stdio.h>
#include <sys/stat.h>

#define PROP(p) printf("%s=%ld\n",#p,st.p)

int main(int argc,char **argv){
	if(argc < 2){
		fprintf(stderr,"not enought arg\n");
	}

	struct stat st;
	if(stat(argv[1],&st) < 0){
		perror(argv[1]);
	}
	
	PROP(st_size);
	PROP(st_uid);
	PROP(st_gid);
	
	return 0;
}