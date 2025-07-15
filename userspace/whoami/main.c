#include <unistd.h>
#include <stdio.h>
#include <pwd.h>

int main(){
	struct passwd *pwd = getpwuid(getuid());
	if(pwd){
		printf("%s\n",pwd->pw_name);
		return 0;
	} else {
		printf("unknow\n");
		return 1;
	}
}