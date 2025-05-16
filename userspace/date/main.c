#include <stdio.h>
#include <time.h>

int main(){
	time_t date = time(NULL);
	printf("%s\n",ctime(&date));
	return 0;
}