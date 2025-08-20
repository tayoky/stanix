#include <stdio.h>

int main(){
		printf("hello world\n");
		puts("integer test");
		printf("decimal : %d, octal : %o, hexa : %x binary : %b\n",-281,281,281,281);
		printf("padding : '%5d' right padding : '%-5d' 0 padding : %05d\n",123,123,123);
		printf("precision : '%.5d' + padding '%10.6i' 0 with 0 precision : '%.d' 0 with default precision : '%i'\n",45,45,0,0);                puts("other flags");                         printf("+ : '%+ld' space : '% hd' # : '%#zx'\n",65L,98,98L);                              puts("lenght");
        printf("%d %hd %hhd %ld %lld %jd %zd %td\n",123,123,123,123L,123LL,123L,123L,123L);
        printf("flags with padding, + : '%+10d' # : '%#10x'\n",42,0x76);
        puts("string and char");
        printf("'%s' %c\n","hello",'t');
        printf("padding : '%10s' '%7c' right padding : '%-10s' '%-7c'\n","test",'a',"test",'a');
        printf("precision : '%.6s'\n","hello world");
        puts("special stuff");
        printf("* : '%*.*d' '%*s'\n",10,5,76,-10,"test");
        return 0;
}