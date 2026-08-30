#include <module/part.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct fs_type {
	gpt_guid_t gpt_uuid;
	uint8_t mbr_uuid;
	char *name;
	char *mount_type;
};

#define arraylen(array)    sizeof(array) / sizeof(*array)
#define GUID(...)          {__VA_ARGS__}
#define FS(n, m, mbr, gpt) {.name = n, .mount_type = m, .mbr_uuid = mbr, .gpt_uuid = gpt}
#define DEV_PATH           "/dev"
#define MNT_PATH           "/mnt"

struct fs_type fs_types[] = {
	FS("EFI system", "fat", 0x00, GUID(0xC12A7328, 0xF81F, 0x11D2, 0xBA4B, {0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B})),
	FS("FAT16", "fat", 0x01, GUID(0)),
	FS("FAT32", "fat", 0x0b, GUID(0)),
	FS("FAT32", "fat", 0x0c, GUID(0)),
};

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
		if (mount(path, path, "part", 0, NULL) < 0) {
fail:
			fprintf(stderr, "\rtry to mount device " DEV_PATH "/%s%d : [fail]\n", prefix, i);
			fprintf(stderr, "%s\n", strerror(errno));
			ret = 1;
			continue;
		}

		for (int j = 0;; j++) {
			sprintf(path, DEV_PATH "/%s%dp%d", prefix, i, j);
			int fd = open(path, O_WRONLY);
			if (fd < 0) break;

			part_info_t info;
			if (ioctl(fd, PART_GET_INFO, &info) < 0) {
				goto fail;
			}
			close(fd);
			struct fs_type *fs = NULL;
			if (info.type == PART_TYPE_MBR) {
				for (size_t i = 0; i < arraylen(fs_types); i++) {
					if (info.mbr.type == fs_types[i].mbr_uuid) {
						fs = &fs_types[i];
						break;
					}
				}
			} else {
				for (size_t i = 0; i < arraylen(fs_types); i++) {
					if (!memcmp(&info.gpt.type, &fs_types[i].gpt_uuid, sizeof(info.gpt.type))) {
						fs = &fs_types[i];
						break;
					}
				}
			}
			if (!fs) {
				fprintf(stderr, "\rtry to mount device " DEV_PATH "/%s%d : [fail]\n", prefix, i);
				if (info.type == PART_TYPE_MBR) {
					fprintf(stderr, "unknow fs type %#x (mbr)\n", info.mbr.type);
				} else {
					fprintf(stderr, "unknow fs type %08x-%04hx-%04hx-%04hx-", info.gpt.type.e1, info.gpt.type.e2, info.gpt.type.e3, info.gpt.type.e4);
					for (int i = 0; i < 6; i++) {
						fprintf(stderr, "%02hhx", info.gpt.type.e5[i]);
					}
					fprintf(stderr, " (gpt)\n");
				}
				ret = 1;
				goto cont;
			}
			char target[PATH_MAX];
			sprintf(target, MNT_PATH "/%s%dp%d", prefix, i, j);
			if (mkdir(target, 0777) < 0) {
				goto fail;
			}
			if (mount(path, target, fs->mount_type, 0, NULL) < 0) {
				goto fail;
			}
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
