#include <stdio.h>
#include <unistd.h>


int main(int argc,char **argv){
	if(geteuid() != 0){
		printf("setuid bit is not set on sudo\n");
	}

	printf("TODO xD\n");

	return 0;
}
