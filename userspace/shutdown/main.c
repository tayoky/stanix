#include <sys/shutdown.h>
#include <string.h>
#include <stdio.h>

void help(void){
	puts("shutdown [OPTIONS]");
	puts("-r --reboot : reboot the machine");
}

int main(int argc,char **argv){
	int flags = 0;

	for (int i=1; i<argc; i++) {
		if (!strcmp(argv[i], "--help")) {
			help();
			return 0;
		} else if (!strcmp(argv[i], "--reboot")) {
			flags |= SHUTDOWN_REBOOT;
		}
	}

	int ret = stanix_shutdown(flags);
	if (ret < 0) {
		perror("shutdown");
		return 1;
	}
	return 0;
}
