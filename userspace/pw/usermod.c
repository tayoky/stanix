#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <pwd.h>
#include <common.h>

int append_groups;
const char *login;
int remove_groups;

static struct option options[] = {
	{"append",     no_argument,       NULL, 'a'},
	{"badname",    no_argument,       NULL, 'b'},
	{"comment",    required_argument, NULL, 'c'},
	{"home",       required_argument, NULL, 'd'},
	{"expiredate", required_argument, NULL, 'e'},
	{"inactive",   required_argument, NULL, 'f'},
	{"gid",        required_argument, NULL, 'g'},
	{"groups",     required_argument, NULL, 'G'},
	{"login",      required_argument, NULL, 'l'},
	{"class",      required_argument, NULL, 'L'},
	{"move-home",  no_argument,       NULL, 'm'},
	{"non-unique", no_argument,       NULL, 'o'},
	{"password",   required_argument, NULL, 'p'},
	{"remove",     no_argument,       NULL, 'r'},
	{"root",       required_argument, NULL, 'R'},
	{"prefix",     required_argument, NULL, 'P'},
	{"shell",      required_argument, NULL, 's'},
	{"uid",        required_argument, NULL, 'u'},
	{"help",       no_argument,       NULL, 'h'},
	{0, 0, 0, 0},
};

static void help(void) {
	puts("usermod [OPTION] LOGIN");
}

int usermod(int argc, char **argv) {
	int opt;
	int option_index;
	while ((opt = getopt_long(argc, argv, "abc:d:e:f:g:G:l:Lmop:rR:P:s:u:h", options, &option_index)) != -1) {
		switch (opt) {
		case 'a':
			append_groups = 1;
			break;
		case 'l':
			check_name(optarg);
			login = optarg;
			break;
		case 'r':
			remove_groups = 1;
			break;
		case 'h':
			help();
			return 0;
		default:
			common_arg(opt);
			break;
		}
	}

	common_setup();
	init_pwd();

	// TODO : update groups and more
	// TODO : do more checking


	struct passwd *pwd;
	int found = 0;
	int ret = 0;
	while ((pwd = get_pwd())) {
		if (strcmp(pwd->pw_name, name)) {
			put_pwd(pwd);
			continue;
		}
		found = 1;
		if (comment) {
			pwd->pw_gecos = (char*)comment;
		}
		if (gid >= 0) {
			pwd->pw_gid = gid;
		}
		if (home) {
			if (create_home) {
				rename(pwd->pw_dir, home);
			}
			pwd->pw_dir = (char*)home;
		}
		if (login) {
			pwd->pw_name = (char*)login;
		}
		if (class) {
			pwd->pw_class = (char*)class;
		}
		if (password) {
			pwd->pw_passwd = (char*)password;
		}
		if (uid >= 0) {
			pwd->pw_uid = uid;
		}
		if (shell) {
			pwd->pw_shell = (char*)shell;
		}
		put_pwd(pwd);
	}

	if (!found) {
		ret = 1;
		fprintf(stderr, "usermod : no such user '%s'\n", name);
		goto error;
	}

	finish_pwd();

	return 0;

error:
	return ret;
}
