#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/block.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DEV_PATH           "/dev"
#define MNT_PATH           "/mnt"

int ret = 0;

void attempt_mount(const char *prefix) {
	char mnt_path[PATH_MAX];
	snprintf(mnt_path, sizeof(mnt_path), MNT_PATH"/%s", prefix);
	char dev_path[PATH_MAX];
	snprintf(dev_path, sizeof(dev_path), DEV_PATH"/%s", prefix);

	if (mkdir(mnt_path, 0777) < 0) return;

	int ret = mount(dev_path, mnt_path, "auto", MS_AUTO, NULL);
	if (ret < 0) {
		rmdir(mnt_path);
	}
}

void check(const char *prefix) {
	char path[PATH_MAX];
	for (char i = 0; i <= 20; i++) {
		sprintf(path, DEV_PATH "/%s%d", prefix, i);
		FILE *f = fopen(path, "r");
		if (!f) continue;
		fclose(f);

		// is it already mounted ?
		sprintf(path, DEV_PATH "/%s%dp0", prefix, i);
		f = fopen(path, "r");
		if (f) {
			// already mounted, ignore
			fclose(f);
			continue;
		}

		fprintf(stderr, "try to mount device " DEV_PATH "/%s%d : ...", prefix, i);

		// we can find partitons
		sprintf(path, DEV_PATH "/%s%d", prefix, i);
		int disk = open(path, O_RDONLY);
		if (disk < 0) goto fail;
		if (ioctl(disk, BLOCK_RESCAN_PARTS) < 0) {
fail:
			fprintf(stderr, "\rtry to mount device " DEV_PATH "/%s%d : [fail]\n", prefix, i);
			fprintf(stderr, "%m\n");
			ret = 1;
			continue;
		}
		
		char dev_prefix[PATH_MAX];
		snprintf(dev_prefix, sizeof(dev_prefix), "%s%d", prefix, i);
		attempt_mount(dev_prefix);

		for (int j = 0;; j++) {
			struct stat buf;
			sprintf(path, DEV_PATH "/%s%dp%d", prefix, i, j);
			if (stat(path, &buf) < 0) break;
			
			char part_prefix[PATH_MAX];
			snprintf(part_prefix, sizeof(part_prefix), "%s%dp%d", prefix, i, j);
			attempt_mount(part_prefix);
		}


		fprintf(stderr, "\rtry to mount device " DEV_PATH "/%s%d : [ok]\n", prefix, i);
cont:
		continue;
	}
}

int main() {
	if (geteuid() != 0) {
		fprintf(stderr, "automount : not run as root\n");
		return 1;
	}
	check("hd");
	check("cd");
	check("nvme");

	return ret;
}
