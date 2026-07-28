#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <pwd.h>
#include <common.h>

int remove_dir;

static struct option options[] = {
	{"remove",     no_argument,       NULL, 'r'},
	{"root",       required_argument, NULL, 'R'},
	{"prefix",     required_argument, NULL, 'P'},
	{"help",       no_argument,       NULL, 'h'},
	{0, 0, 0, 0},
};

static void help(void) {
	puts("usermod [OPTION] LOGIN");
}

int userdel(int argc, char **argv) {
	int opt;
	int option_index;
	while ((opt = getopt_long(argc, argv, "R:P:h", options, &option_index)) != -1) {
		switch (opt) {
		case 'r':
			remove_dir = 1;
			break;
		case 'h':
			help();
			break;
		default:
			common_arg(opt);
			break;
		}
	}

	common_setup();
	init_pwd();

	struct passwd *pwd;
	while ((pwd = get_pwd())) {
		if (!strcmp(name, pwd->pw_name)) {
			if (remove_dir) {
				pid_t child = fork();
				if (!child) {
					execlp("rm", "rm", "-fr", pwd->pw_dir, NULL);
					exit(1);
				}
				waitpid(child, NULL, 0);
			}
			continue;
		}
		put_pwd(pwd);
	}
	finish_pwd();
	return 0;
}
