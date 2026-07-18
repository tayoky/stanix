#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <getopt.h>
#include <syslog.h>

const char *autologin;
int noreset = 0;
int noissue = 0;
int noclear = 0;
int skip_login = 0;

void reset(void) {
	struct termios attr;
	if (tcgetattr(STDOUT_FILENO, &attr) < 0) {
		syslog(LOG_WARNING, "could not get termios attributes : %m");
		return;
	}

	attr.c_oflag |= OPOST | ONLCR;
	attr.c_lflag |= ISIG | ICANON | ECHO | ECHOE | ECHOK;

	if (tcsetattr(STDOUT_FILENO, TCIOFLUSH, &attr) < 0) {
		syslog(LOG_WARNING, "could not set termios attributes : %m");
	}
}

void clear(void) {
	printf("\033[2J");
}


void show_issue(void) {
	int fd = open("/etc/issue", O_RDONLY);
	if (fd < 0) {
		// no issue
		return;
	}

	char buf[4096];
	ssize_t r;
	int prev = '\0';
	while ((r = read(fd, buf, sizeof(buf)))) {
		for (size_t i=0; i<r; i++) {
			if (prev == '\\') {
				switch (buf[i]) {
				// TODO : add more
				case 's':
					printf("Stanix");
					break;
				case 'm':
#ifdef __x86_64__
					printf("x86_64");
#elif defined(__i386__)
					printf("i386");
#elif defined(__aarch64__)
					printf("aarch64");
#else
					printf("unknown");
#endif
					break;
				}
			} else {
				putchar(buf[i]);
			}
			prev = buf[i];
		}
	}
	close(fd);
}

struct option options[] = {
	{"autologin",  required_argument, NULL, 'a'},
	{"noreset",    no_argument,       NULL, 'c'},
	{"noissue",    no_argument,       NULL, 'i'},
	{"noclear",    no_argument,       NULL, 'J'},
	{"skip-login", no_argument,       NULL, 'n'},
	{0, 0, 0, 0},
};

int main(int argc, char **argv) {
	int opt;
	int opt_index;
	opterr = 0;
	while ((opt = getopt_long(argc, argv, "a:ciJn", options, &opt_index)) != -1) {
		switch (opt) {
		case 'a':
			autologin = optarg;
			break;
		case 'c':
			noreset = 1;
			break;
		case 'i':
			noissue = 1;
			break;
		case 'J':
			noclear = 1;
			break;
		case 'n':
			skip_login = 1;
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

	int i = optind;
	if (i >= argc) {
		fprintf(stderr, "getty : no port specified\n");
		return 1;
	}

	openlog("getty", LOG_CONS | LOG_PID, LOG_AUTH);

	const char *port = argv[i++];
	const char *term = "vt100";
	if (i < argc) {
		term = argv[i++];
	}

	syslog(LOG_INFO, "starting tty login on port '%s'", port);

	if (strcmp(port, "-")) {
		// we need to setup
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "/dev/%s", port);

		int tty = open(path, O_RDWR);
		if (tty < 0) {
			syslog(LOG_ERR, "failed to open '%s' : %m", path);
			fprintf(stderr, "getty : failed to open '%s'\n", path);
			return 1;
		}
		dup2(tty, STDIN_FILENO);
		dup2(tty, STDOUT_FILENO);
		dup2(tty, STDERR_FILENO);
		if (tty > STDERR_FILENO) close(tty);
	}

	setenv("TERM", term, 1);

	if (!noreset) reset();
	if (!noclear) clear();
	if (!noissue) show_issue();

	if (autologin) {
		execlp("login", "login", "-f", autologin, NULL);
	} else {
		execlp("login", "login", NULL);
	}
	syslog(LOG_ERR, "failed to exec login : %m");
	return 1;
}
