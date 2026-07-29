#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <common.h>

int badname;
const char *comment;
const char *home;
mode_t home_mode = 0755;
const char *expire_date;
const char *inactive;
gid_t gid = -1;
char **groups;
const char *class;
int create_home;
int non_unique;
const char *password;
const char *root = "";
const char *prefix = "";
const char *shell;
uid_t uid = -1;
const char *program_name;
const char *name;
uid_t min_uid = 1000;
uid_t max_uid = 60000;

void error(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "%s : ", program_name);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
}

void common_setup(void) {
	name = _argv[optind];
	if (!name) {
		error("missing login name");
		exit(2);
	}
	if (!non_unique) {
		if (uid >= 0) {
			struct passwd *pwd = getpwuid(uid);
			if (pwd && strcmp(pwd->pw_name, name)) {
				error("non unique uid %ld", uid);
				exit(1);
			}
		}
	}
}

void check_name(const char *name) {
	if (!name) return;
	if (badname) return;
	if (strlen(name) > 31) {
		error("login name too long '%s'", name);
		exit(1);
	}
	if (!isalpha(*name)) {
		error("invalid first character for login name '%s'", name);
		exit(1);
	}
	while (name) {
		if (!isalnum(*name) && *name != '-' && *name != '_') {
			error("invalid character for login name '%c'", *name);
		}
		name++;
	}
}

static long str2id(const char *str, const char *type) {
	char *end;
	long id = strtol(str, &end, 10);
	if (end == str || *end) {
		error("invalid %s : '%s'\n", type, str);
		exit(3);
	}
	return id;
}

void common_arg(int opt) {
	switch (opt) {
		case 'b':
			badname = 1;
			break;
		case 'c':
			comment = optarg;
			break;
		case 'd':
			home = optarg;
			break;
		case 'e':
			expire_date = optarg;
			break;
		case 'f':
			inactive = optarg;
			break;
		case 'g':
			gid = str2id(optarg, "gid");
			break;
		case 'G':
			// TODO
			break;
		case 'L':
			class = optarg;
			break;
		case 'm':
			create_home = 1;
			break;
		case 'o':
			non_unique = 1;
			break;
		case 'p':
			password = optarg;
			break;
		case 'R':
			root = optarg;
			break;
		case 'P':
			prefix = optarg;
			break;
		case 's':
			shell = optarg;
			break;
		case 'u':
			uid = str2id(optarg, "uid");
			break;
		case '?':
			exit(2);
			break;
	}
}
