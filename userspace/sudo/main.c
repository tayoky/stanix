#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>


int main(int argc,char **argv){
	if(geteuid() != 0){
		printf("setuid bit is not set on sudo\n");
	}

	if(argc < 2){
		fprintf(stderr,"missing argument\n");
		return 1;
	}

	if(!strcmp(argv[1],"--shell") || !strcmp(argv[1],"-s")){
		char *shell = getenv("SHELL");
		if(!shell){
			shell = "sh";
		}

		char *args[] = {
			shell,
			NULL
		};

		execvp(args[0],args);
		perror(args[0]);
		return 1;
	}
	if(!strcmp(argv[1],"--help")){
		printf("usage : sudo COMMAND or\n");
		printf("sudo OPTION\n");
		printf("run a command as root\n");
		printf("-s --shell : run a shell instead of a command\n");
		return 0;
	}

	execvp(argv[1],&argv[1]);
	perror(argv[1]);

	return 0;
}
