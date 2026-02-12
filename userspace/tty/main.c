#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

int main(){
	char *tty = fdname(STDERR_FILENO);
	if (!tty) {
		return 1;
	}
	puts(tty);
	return 0;
}