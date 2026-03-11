#include <stdio.h>
#include <dlfcn.h>

int main(){
	void *libc = dlopen("libc.so", RTLD_NOW);
	int (*libc_puts)(const char *) = dlsym(libc, "puts");
	libc_puts("hello world with dlsym !");
	return 0;
}