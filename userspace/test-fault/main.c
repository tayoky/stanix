#include <unistd.h>

int main(){
	read(STDIN_FILENO, (void*)0x1234, 10);
	return 0;
}