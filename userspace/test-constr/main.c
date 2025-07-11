#include <stdio.h>
void __attribute__((constructor)) test(void);
void test(void){
	printf("constructors called !\n");
}
int main(){
	return 0;
}