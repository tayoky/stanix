#include <unistd.h>
#include <stdio.h>

int main(){
	char *tty = ttyname(STDERR_FILENO);
	if (!tty) {
		return 1;
	}
	puts(tty);
	return 0;
}