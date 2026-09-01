#include <sys/mount.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "initrd-init : missing argument\n");
		return 1;
	}

	const char *root = NULL;
	char *arg = strtok(argv[1], " ");
	while (arg) {
		const char *value = strchr(arg, '=');
		if (value) {
			*value = '\0';
			value++;
		}
		if (!strcmp(arg, "--root")) {
			if (!value) {
				value = strtok(NULL, " ");
			}
			if (!value) {
				fprintf(stderr, "initrd-init : option --root require an argument");
				return 1;
			}
			root = value;
			break;
		}
		arg = strtok(NULL, " ");
	}

	if (!root) {
		fprintf(stderr, "initrd-init : no root specified\n");
		return 1;
	}

	mkdir("/mnt", 0777);
	if (mount(root, "/mnt", "auto", MS_AUTO, NULL) < 0) {
		fprintf(stderr, "initrd-init : failed to mount '%s' : %m\n", root);
		return 1;
	}


	int fd = open("/mnt");
	if (fd < 0) {
		fprintf(stderr, "initrd-init : failed to open '/mnt': %m\n");
		return 1;
	}

	// move /mnt over root
	mount("/mnt", "/", "", MS_MOVE, NULL);

	// jump into the new root
	fchroot(fd);
	fchdir(fd);

	execl("/bin/init", "/bin/init", argv[1], NULL);
	fprintf(stderr, "initrd-init : failed to exec '/bin/init': %m\n");

	return 1;
}
