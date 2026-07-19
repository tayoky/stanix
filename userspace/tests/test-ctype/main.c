#include <stdio.h>
#include <ctype.h>
#include <string.h>

typedef struct {
	int (*func)(int);
	int c;
	int result;
} test;

test tests[] = {
	{.func = tolower,.c = 'A',.result = 'a'},
	{.func = toupper,.c = 'a',.result = 'A'},
	{.func = NULL}
};

int main(){
	int i = 0;
	while(tests[i].func){
		if(tests[i].func(tests[i].c) != tests[i].result){
			fprintf(stderr,"test %d failed\n",i);
			return 1;
		}
		i++;
	}

	//now test strx function
	if(strcasecmp("HEllO","hello")){
		printf("test stricmp  failed\n");
		return 1;
	}
	return 0;
}
