#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <common.h>

int badname;
const char *comment;
const char *home;
const char *expire_date;
const char *inactive;
gid_t gid = -1;
char **groups;
const char *class;
int create_home;
int non_unique;
const char *password;
const char *root = "/";
const char *prefix = "/";
const char *shell;
uid_t uid = -1;
const char *program_name;
const char *name;

void common_setup(void) {
	name = _argv[optind];
	if (!name) {
		fprintf(stderr, "%s : missing login name\n", program_name);
		exit(2);
	}
	if (!non_unique) {
		if (uid >= 0) {
			struct passwd *pwd = getpwuid(uid);
			if (pwd && strcmp(pwd->pw_name, name)) {
				fprintf(stderr, "%s : non unique uid %ld\n", program_name, uid);
				exit(1);
			}
		}
	}
}

void check_name(const char *name) {
	if (!name) return;
	if (badname) return;
	if (strlen(name) > 31) {
		fprintf(stderr, "%s : login name too long '%s'\n", program_name, name);
		exit(1);
	}
	if (!isalpha(*name)) {
		fprintf(stderr, "%s : invalid first character for login name '%s'\n", program_name, name);
		exit(1);
	}
	while (name) {
		if (!isalnum(*name) && *name != '-' && *name != '_') {
			fprintf(stderr, "%s invalid character for login name '%c'\n", program_name, *name);
		}
		name++;
	}
}

static long str2id(const char *str, const char *type) {
	char *end;
	long id = strtol(str, &end, 10);
	if (end == str || *end) {
		fprintf(stderr, "%s : invalid %s : '%s'\n", program_name, type, str);
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
