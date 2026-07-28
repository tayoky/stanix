#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <common.h>

char **_argv;

int main(int argc, char **argv) {
	_argv = argv;
	program_name = argv[0];
	if (strchr(program_name, '/')) {
		program_name = strrchr(program_name, '/') + 1;
	}

	// prevent user read by default
	umask(0077);

	if (!strcmp(program_name, "usermod")) {
		return usermod(argc, argv);
	} else if (!strcmp(program_name, "userdel")) {
		return userdel(argc, argv);
	} else {
		fprintf(stderr, "%s : unknown command\n", program_name);
		return 1;
	}
}
