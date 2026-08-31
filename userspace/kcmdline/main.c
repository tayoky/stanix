#include <stdio.h>
#include <string.h>


int main(int argc, char **argv) {
	if (argc < 2) {
		puts("usage : kcmdline OPTION...");
		puts("small utility to check if kernel cmdline has a specfied option");
		puts("if the option has an argument it is printed to stdout");
		return 1;
	}

	FILE *cmdline_file = fopen("/sys/kernel/cmdline", "r");
	if (!cmdline_file) {
		perror("/sys/kernel/cmdline");
		return 1;
	}
	char cmdline[256];
	fgets(cmdline, sizeof(cmdline), cmdline_file);

	char *arg = strtok(cmdline, " ");
	while (arg) {
		char *value = strchr(arg, '=');
		if (value) {
			*value = '\0';
			value++;
		}

		for (int i = 1; i < argc; i++) {
			if (!strcmp(arg, argv[i])) {
				if (value) puts(value);
				return 0;
			}
		}

		arg = strtok(NULL, " ");
	}

	return 1;
}
