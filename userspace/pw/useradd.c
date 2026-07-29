#include <sys/stat.h>
#include <getopt.h>
#include <string.h>
#include <common.h>

const char *skel = "/usr/share/skel";

static struct option options[] = {
	{"badname",     no_argument,       NULL, 'b'},
	{"comment",     required_argument, NULL, 'c'},
	{"home",        required_argument, NULL, 'd'},
	{"expiredate",  required_argument, NULL, 'e'},
	{"inactive",    required_argument, NULL, 'f'},
	{"gid",         required_argument, NULL, 'g'},
	{"groups",      required_argument, NULL, 'G'},
	{"skel",        required_argument, NULL, 'k'},
	{"class",       required_argument, NULL, 'L'},
	{"create-home", no_argument,       NULL, 'm'},
	{"non-unique",  no_argument,       NULL, 'o'},
	{"password",    required_argument, NULL, 'p'},
	{"root",        required_argument, NULL, 'R'},
	{"prefix",      required_argument, NULL, 'P'},
	{"shell",       required_argument, NULL, 's'},
	{"uid",         required_argument, NULL, 'u'},
	{"help",        no_argument,       NULL, 'h'},
	{0, 0, 0, 0},
};

static void help(void) {
	puts("useradd [OPTION] LOGIN");
}

int useradd(int argc, char **argv) {
	int opt;
	int option_index;
	while ((opt = getopt_long(argc, argv, "bc:d:e:f:g:G:k:Lmop:R:P:s:u:h", options, &option_index)) != -1) {
		switch (opt) {
		case 'k':
			skel = optarg;
		case 'h':
			help();
			break;
		default:
			common_arg(opt);
			break;
		}
	}

	common_setup();

	load_conf();

	if (create_home) {
		mkdir(home, home_mode);
		// TODO : copy skel
	}

	bitmap_t *bitmap = bitmap_create(max_uid - min_uid);

	init_pwd();
	struct passwd *pwd;
	while ((pwd = get_pwd())) {
		bitmap_set(bitmap, pwd->pw_uid - min_uid);
		if (!strcmp(pwd->pw_name, name)) {
			error("user '%s' already exist", name);
			return 1;
		}
		put_pwd(pwd);
	}

	if (uid < 0) {
		uid = bitmap_allocate(bitmap) + min_uid;
	}
	if (gid < 0) {
		gid = uid;
	}
	struct passwd new_pwd = {
		.pw_name   = (char*)name,
		.pw_passwd = (char*)password,
		.pw_uid    = uid,
		.pw_gid    = gid,
		.pw_class  = (char*)class,
		.pw_gecos  = (char*)comment,
		.pw_dir    = (char*)home,
		.pw_shell  = (char*)shell,
	};
	put_pwd(&new_pwd);
	finish_pwd();

	return 0;
}
