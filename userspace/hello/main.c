#include <stdio.h>

void __tlibc_init();

int main(int argc,char **argv){
	__tlibc_init();

	printf("hello world\n");
	printf("argc is : %d\n",argc);
	for (size_t i = 0; i < argc; i++){
		printf("arg %d : %s\n",argc,argv[i]);
	}
	
	return 0;
}

