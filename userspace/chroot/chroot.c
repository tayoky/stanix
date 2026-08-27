#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>

int skip_chdir = 0;

struct option options[] = {
	{"skip-chdir", no_argument, NULL, 's'},
	{"help",       no_argument, NULL, 'h'},
	{0, 0, 0, 0},
};

void help(void) {
	puts("chroot [OPTIONS] NEWROOT [COMMAND [ARGS]]");
	puts("chroot into NEWROOT and launch a shell or execute a command");
	puts("-s --skip-chdir do not chdir into /");
}

int main(int argc, char **argv) {
	int opt;
	int option_index;
	while ((opt = getopt_long(argc, argv, "sh", options, &option_index)) != -1) {
		switch (opt) {
		case 's':
		skip_chdir = 1;
			break;
		case 'h':
			help();
			return 0;
		case '?':
			return 1;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "chroot : missing argument\n");
		return 1;
	}

	if (chroot(argv[optind]) < 0) {
		fprintf(stderr, "chroot : %s : %m\n", argv[optind]);
		return 1;
	}

	if (!skip_chdir) {
		chdir("/");
	}

	optind++;

	if (optind >= argc) {
		// launch shell
		const char *shell = getenv("SHELL");
		if (!shell) shell = "/bin/sh";

		execlp(shell, shell, NULL);
		fprintf(stderr, "chroot : %s : %m\n", shell);
		return 1;
	} else {
		execvp(argv[optind], &argv[optind]);
		fprintf(stderr, "chroot : %s : %m\n", argv[optind]);
		return 1;
	}
	return 0;
}