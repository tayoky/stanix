#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

struct option options[] = {
	{"shell", no_argument, NULL, 's'},
	{"help",  no_argument, NULL, 'h'},
	{0, 0, 0, 0},
};

void help(void) {
	puts("usage : sudo COMMAND or");
	puts("sudo OPTION");
	puts("run a command as root");
	puts("-s --shell : run a shell instead of a command");
}

int main(int argc, char **argv) {
	if (geteuid() != 0) {
		printf("sudo : setuid bit is not set on sudo\n");
		return 1;
	}

	int opt_index;
	int option;
	int do_shell = 0;
	opterr = 0;
	while ((opt = getopt_long(argc, argv, "sh", options, &opt_index)) != -1) {
		switch (opt) {
		case 's':
			do_shell = 1;
			break;
		case 'h':
			help();
			return 0;
		case '?':
			if (optopt) {
				fprintf(stderr, "sudo : invalid option '-%c'\n", optopt);
			} else {
				fprintf(stderr, "sudo : invalid option '%s'\n", argv[optind-1]);
			}
			return 1;
		}
	}

	// TODO : check password and perm here

	if (do_shell) {
		char *shell = getenv("SHELL");
		if (!shell) {
			shell = "sh";
		}

		char *args[] = {
			shell,
			NULL
		};

		execvp(args[0], args);
		perror(args[0]);
		return 1;
	}

	if (optind >= argc) {
		fprintf(stderr, "sudo : missing argument\n");
		return 1;
	}

	execvp(argv[optind], &argv[optind]);
	perror(argv[optind]);

	return 0;
}
