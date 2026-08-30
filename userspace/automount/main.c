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

		for (int j = 0;; j++) {
			sprintf(path, DEV_PATH "/%s%dp%d", prefix, i, j);
			int fd = open(path, O_WRONLY);
			if (fd < 0) break;

			block_part_info_t info;
			if (ioctl(fd, BLOCK_GET_PART_INFO, &info) < 0) {
				goto fail;
			}
			close(fd);
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
