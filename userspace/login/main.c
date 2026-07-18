#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

int preserve;
int autologin;
const char *shell;
const char *user;
struct passwd *pwd = NULL;

struct option options = {
	{"preserve",  no_argument,       NULL, 'p'},
	{"autologin", no_argument,       NULL, 'f'},
	{"shell",     argument_required, NULL, 's'},
	{0,		   0,				 0,    0  },
};

int login_loop(void) {
	if (!pwd) {
		fprintf(stderr, "username : ");
		fflush(stdout);
		char username[256];
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
			// TODO : check called as root
			autologin = 1;
			break;
		case 's':
			// TODO
			break;
		case '?':
			if (optopt) {
				fprintf(stderr, "getty : invalid option '-%c'\n", optopt);
			} else {
				fprintf(stderr, "getty : invalid option '%s'\n", argv[optind-1]);
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
			pwd = getpwnam(user),
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
	char logname[256];
	char home[256];
	char shell[256];
	snprintf(logname, sizeof(logname), "LOGNAME=%s", pwd->pw_name);
	snprintf(home, sizeof(home), "HOME=%s", pwd->pw_dir);
	snprintf(shell, sizeof(shell), "SHELL=%s", pwd->pw_shell);
	putenv(logname);
	putenv(home);
	putenv(shell);
	putenv("PATH=/bin:/usr/bin");

	setgid(pwd->pw_gid);
	setuid(pwd->pw_uid);

	chdir(pwd->pw_dir);

	// clear screen and print /motd
	system("cat /etc/motd");

	char *arg[] = {
		pwd->pw_shell,
		NULL};
	execv(pwd->pw_shell, arg);
	perror(pwd->pw_shell);

	return 1;
}
