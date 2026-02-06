#include <stdio.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <pwd.h>

#define SELECT_CLASSIC   0
#define SELECT_NLEAD_TTY 1 //all wth attached tty
#define SELECT_NLEADER   2
#define SELECT_ALL       3

int ret = 0;
int select = SELECT_CLASSIC;

char *uid2name(uid_t uid) {
	struct passwd *pwd = getpwuid(uid);
	if (!pwd) {
		static char buf[64];
		sprintf(buf, "%ld", uid);
		return buf;
	}
	return pwd->pw_name;
}

void print(const char *path, const char *pid) {
	struct stat st;
	if (stat(path, &st) < 0) {
		perror(path);
		ret = 1;
		return;
	}
	char cmdline[256];
	char cmdline_path[256];
	snprintf(cmdline_path, sizeof(cmdline_path), "%s/cmdline", path);
	int cmdline_fd = open(cmdline_path, O_RDONLY);
	if (cmdline_fd >= 0) {
		ssize_t size = read(cmdline_fd, cmdline, sizeof(cmdline) - 1);
		if (size < 0) {
			perror(cmdline_path);
			ret = 1;
			close(cmdline_fd);
		}
		cmdline[size] = '\0';
		close(cmdline_fd);
	} else {
		// on some olders stanix versions, cmdline does not exist
		strcpy(cmdline, "unknow");
	}
	printf("%-10s %5s %s\n", uid2name(st.st_uid), pid, cmdline);
}


int main(int argc, char **argv) {
	for (int i=1; i < argc; i++) {
		if (argv[i][0] != '-') {
			fprintf(stderr, "ps : invalid option '%s'\n", argv[i]);
			return 1;
		}
	}
	DIR *proc = opendir("/proc");
	if (!proc) {
		perror("/proc");
		return 1;
	}

	for (;;) {
		struct dirent *ent = readdir(proc);
		if (!ent)break;
		if (!isdigit(ent->d_name[0]))continue;

		char path[PATH_MAX + 6];
		snprintf(path, sizeof(path), "/proc/%s", ent->d_name);
		print(path, ent->d_name);
	}

	closedir(proc);

	return ret;
}