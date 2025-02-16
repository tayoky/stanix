#include <fcntl.h>
#include <syscall.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

void __tlibc_init();

int main(){
	__tlibc_init();

	//init std streams
	open("dev:/null",O_RDONLY); //stdin
	open("dev:/tty0",O_WRONLY); //stdout
	open("dev:/tty0",O_WRONLY); //stderr

	printf("hello world !!\n");
	int fd = open("tmp:/test.txt",O_CREAT |O_RDWR);
	
	//assert(5 == 3);

	return 0;
}

