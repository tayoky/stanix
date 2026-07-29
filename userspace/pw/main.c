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
	const char *command = program_name;

	// prevent user read by default
	umask(0077);

	if (!strcmp(program_name, "pw")) {
		if (argc < 2) {
			fprintf(stderr, "pw : no command specified");
		}
		argv++;
		argc--;
		command = argv[0];
	}

	if (!strcmp(command, "usermod")) {
		return usermod(argc, argv);
	} else if (!strcmp(command, "useradd")) {
		return useradd(argc, argv);
	} else if (!strcmp(command, "userdel")) {
		return userdel(argc, argv);
	} else {
		fprintf(stderr, "%s : unknown command '%s'\n", program_name, command);
		return 1;
	}
}
