#include <fcntl.h>
#include <syscall.h>
#include <unistd.h>
#include <string.h>

int main(){
	int fd = open("dev:/tty0",O_WRONLY);
	char hello[] = "hello world !!\n";
	write(fd,hello,strlen(hello));
	return 0;
}