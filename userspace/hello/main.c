#include <stdio.h>

int main(int argc,char **argv){

	printf("hello world\n");
	printf("argc is : %d\n",argc);
	for (size_t i = 0; i < argc; i++){
		printf("arg %d : %s\n",argc,argv[i]);
	}
	
	return 0;
}
