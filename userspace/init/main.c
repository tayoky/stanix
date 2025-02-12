#include <fcntl.h>
#include <syscall.h>
#include <unistd.h>
#include <string.h>

int main(){
	//init std streams
	open("dev:/null",O_RDONLY); //stdin
	open("dev:/tty0",O_WRONLY); //stdout
	open("dev:/tty0",O_WRONLY); //stderr

	char hello[] = "hello world !!\n";
	write(STDOUT_FILENO,hello,strlen(hello));
	return 0;
}