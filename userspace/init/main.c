#include <fcntl.h>
#include <syscall.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void __tlibc_init();

int main(){
	__tlibc_init();

	//init std streams
	open("dev:/null",O_RDONLY); //stdin
	open("dev:/tty0",O_WRONLY); //stdout
	open("dev:/tty0",O_WRONLY); //stderr

	printf("hello world !!\n");

	return 0;
}

