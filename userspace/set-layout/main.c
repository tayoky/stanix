#include <stdio.h>
#include <string.h>
#include <libinput.h>

int main(int argc, char **argv) {
	if (argc >= 2) {
		if (!strcmp(argv[1], "--help")) {
			puts("usage : set-layout DEVICE LAYOUT");
			puts("set the layout of a keybaord");
			return 0;
		}
	}
	if (argc < 3) {
		puts("usage : set-layout DEVICE LAYOUT");
		return 1;
	}
	if (strlen(argv[2]) > INPUT_LAYOUT_SIZE - 1) {
		fprintf(stderr, "layout name is too long, cannot be bigger than %d\n", INPUT_LAYOUT_SIZE);
		return 1;
	}
	int fd = libinput_open(argv[1], 0);
	if (fd < 0) {
		perror(argv[1]);
		return 1;
	}
	if (libinput_set_layout(fd, argv[2])) {
		perror(argv[1]);
		return 1;
	}
	return 0;
}