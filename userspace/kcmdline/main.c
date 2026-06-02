#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
	if (argc < 2) {
		puts("usage : kcmdline OPTION");
		puts("small utility to check if kernel cmdline has a specfied option");
		return 1;
	}

	FILE *cmdline_file = fopen("/sys/kernel/cmdline", "r");
	if (!cmdline_file) {
		perror("/sys/kernel/cmdline");
		return 1;
	}
	char cmdline[256];
	fgets(cmdline, sizeof(cmdline), cmdline_file);

	for (int i = 1; i < argc; i++) {
		if (strstr(cmdline, argv[i])) {
			return 0;
		}
	}

	return 1;
}
