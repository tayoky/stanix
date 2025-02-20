#include <stdio.h>

void __tlibc_init();

int main(){
	__tlibc_init();

	printf("hello world\n");
	return 0;
}

