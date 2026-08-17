#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <syslog.h>
#include <getopt.h>

int preserve;
int autologin;
char *shell;
const char *user;
struct passwd *pwd = NULL;

struct option options[] = {
	{"preserve", no_argument,       NULL, 'p'},
	{"force",    no_argument,       NULL, 'f'},
	{"shell",    required_argument, NULL, 's'},
	{"help",     no_argument,       NULL, 'h'},
	{0, 0, 0, 0},
};

void help(void) {
	puts("usage : login [OPTIONS] [[-f] username|uid]");
	puts("ask for login and start session");
	puts("-p --preserve : preserve environ");
	puts("-f --force    : autologin");
	puts("-s --shell    : specify a custom shell");
}

void login_loop(void) {
	char username[256];
	if (!pwd) {
		fprintf(stderr, "username : ");
		fflush(stdout);
		fgets(username, sizeof(username), stdin);
		if (strchr(username, '\n')) *strrchr(username, '\n') = '\0';
		pwd = getpwnam(username);
	}
	if (pwd && !strcmp(pwd->pw_passwd, "")) {
		// no password
		return;
	}

	int succed = 0;
	for (int i=0; i<3; i++) {
		fprintf(stderr, "password : ");
		fflush(stdout);
		// disable echo
		static struct termios old, new;
		if (isatty(STDIN_FILENO) == 1) {
			tcgetattr(STDIN_FILENO, &old);
			new = old;
			new.c_lflag &= ~ECHO;
			tcsetattr(STDIN_FILENO, TCSANOW, &new);
		}
		char password[256];
		fgets(password, sizeof(password), stdin);
		if (isatty(STDIN_FILENO) == 1) {
			tcsetattr(STDIN_FILENO, TCSANOW, &old);
		}
		putchar('\n');
		if (strrchr(password, '\n')) *strrchr(password, '\n') = '\0';

		if (!pwd || strcmp(pwd->pw_passwd, password)) {
			fprintf(stderr, "login : wrong username or password\n");
			syslog(LOG_NOTICE, "failed login for user '%s'", pwd ? pwd->pw_name : username);
			sleep(2);
		} else {
			succed = 1;
			break;
		}
	}
	if (!succed) {
		fprintf(stderr, "login : login failed\n");
		exit(1);
	}
	syslog(LOG_INFO, "successful login for user '%s'", pwd->pw_name);
}

int main(int argc, char **argv) {
	if (geteuid() != 0) {
		fprintf(stderr, "login : must be run as root\n");
		return 1;
	}

	int opt = 0;
	int opt_index;
	opterr =  0;
	while ((opt = getopt_long(argc, argv, "pfs:", options, &opt_index)) != -1) {
		switch (opt) {
		case 'p':
			preserve = 1;
			break;
		case 'f':
			if (getuid() != 0) {
				fprintf(stderr, "login : invalid privilege for option '-f'/'--force'\n");
				return 1;
			}
			autologin = 1;
			break;
		case 's':
			shell = optarg;
			break;
		case 'h':
			help();
			return 0;
		case '?':
			if (optopt) {
				fprintf(stderr, "login : invalid option '-%c'\n", optopt);
			} else {
				fprintf(stderr, "login : invalid option '%s'\n", argv[optind-1]);
			}
			return 1;
		}
	}

	if (optind < argc) {
		user = argv[optind];
	}

	if (user) {
		char *end;
		uid_t uid = (uid_t)strtol(user, &end, 10);
		if (!*end) {
			pwd = getpwuid(uid);
		} else {
			pwd = getpwnam(user);
		}
		if (!pwd) {
			fprintf(stderr, "login : invalid user '%s'\n", user);
			return 1;
		}
	}
	if (argc >= 2 && !strcmp(argv[1], "-f")) {
		pwd = getpwuid(getuid());
		if (!pwd) {
			fprintf(stderr, "login : can't skip login\n");
			return 1;
		}
	} else {
	}

	openlog("login", LOG_CONS | LOG_PID, LOG_AUTH);

	if (autologin) {
		if (!pwd) {
			fprintf(stderr, "login : cannot autologin without specifing user\n");
			return 1;
		}
		syslog(LOG_INFO, "successful autologin for user '%s'", pwd->pw_name);
	} else {
		login_loop();
	}

	// setup an env
	setenv("LOGNAME", pwd->pw_name,  1);
	setenv("HOME",    pwd->pw_dir,   1);
	setenv("SHELL",   pwd->pw_shell, 1);
	putenv("PATH=/bin:/usr/bin:/usr/local/bin");

	setgid(pwd->pw_gid);
	setuid(pwd->pw_uid);

	chdir(pwd->pw_dir);

	// print the motd
	system("cat /etc/motd");

	if (!shell) shell = pwd->pw_shell;

	char *arg[] = {
		shell,
		NULL
	};
	execv(shell, arg);
	perror(shell);

	return 1;
}
