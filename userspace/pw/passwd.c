#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <pwd.h>
#include <common.h>

static char passwd_path[PATH_MAX];
static char passwd_tmp_path[PATH_MAX];
static char master_path[PATH_MAX];
static char master_tmp_path[PATH_MAX];
static FILE *passwd_tmp;
static FILE *master;
static FILE *master_tmp;

static void master_putpwent(struct passwd *pwd, FILE *stream) {
	fprintf(stream, "%s:%s:%d:%d:%s:%ld:%ld:%s:%s:%s\n",
			pwd->pw_name, pwd->pw_passwd, pwd->pw_uid, pwd->pw_gid,
			pwd->pw_class, pwd->pw_change, pwd->pw_expire,
			pwd->pw_gecos, pwd->pw_dir, pwd->pw_shell);
}

static long parse_long(const char *str) {
	char *end;
	long l = strtol(str, &end, 10);
	if (str == end || *end) {
		fprintf(stderr, "%s : invalid master.passwd entry\n", program_name);
		exit(1);
	}
	return l;
}

static struct passwd *master_fgetpwent(FILE *stream) {
	static struct passwd pwd;
	static char buf[1024];
	if (!fgets(buf, sizeof(buf), stream)) return NULL;
	char *ptr = buf;
	pwd.pw_name   = strsep(&ptr, ":");
	pwd.pw_passwd = strsep(&ptr, ":");
	pwd.pw_uid    = parse_long(strsep(&ptr, ":"));
	pwd.pw_gid    = parse_long(strsep(&ptr, ":"));
	pwd.pw_class  = strsep(&ptr, ":");
	pwd.pw_change = parse_long(strsep(&ptr, ":"));
	pwd.pw_expire = parse_long(strsep(&ptr, ":"));
	pwd.pw_gecos  = strsep(&ptr, ":");
	pwd.pw_dir    = strsep(&ptr, ":");
	pwd.pw_shell  = strsep(&ptr, ":\n");
	if (!pwd.pw_shell) {
		fprintf(stderr, "%s : invalid master.passwd entry\n", program_name);
		exit(1);
	}
	return &pwd;
}

static void passwd_putpwent(struct passwd *pwd, FILE *stream) {
	fprintf(stream, "%s:x:%d:%d:%s:%s:%s\n",
			pwd->pw_name, pwd->pw_uid, pwd->pw_gid,
			pwd->pw_gecos, pwd->pw_dir, pwd->pw_shell);
}

struct passwd *get_pwd(void) {
	return master_fgetpwent(master);
}

void put_pwd(struct passwd *pwd) {
	master_putpwent(pwd, master_tmp);
	passwd_putpwent(pwd, passwd_tmp);
}

static FILE *open_file(const char *path, const char *mode) {
	FILE *file = fopen(path, mode);
	if (!file) {
		fprintf(stderr, "%s : %s : %m\n", program_name, path);
		exit(1);
	}
	return file;
}

void init_pwd(void) {
	snprintf(passwd_path, sizeof(passwd_path), "%s%s/etc/passwd", root, prefix);
	snprintf(passwd_tmp_path, sizeof(passwd_tmp_path), "%s%s/etc/passwd.tmp", root, prefix);
	snprintf(master_path, sizeof(master_path), "%s%s/etc/master.passwd", root, prefix);
	snprintf(master_tmp_path, sizeof(master_tmp_path), "%s%s/etc/master.passwd.tmp", root, prefix);
	passwd_tmp = open_file(passwd_tmp_path, "w");
	master     = open_file(master_path,     "r");
	master_tmp = open_file(master_tmp_path, "w");
}

void finish_pwd(void) {
	// allow user read
	fchmod(fileno(passwd_tmp), 0644);
	rename(passwd_tmp_path, passwd_path);
	rename(master_tmp_path, master_path);
}
