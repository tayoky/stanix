#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <stdio.h>

int data = 0;
int file_system = 0;

struct option options[] = {
	{"data",        no_argument, NULL, 'd'},
	{"file-system", no_argument, NULL, 'f'},
	{"help",        no_argument, NULL, 'h'},
	{0, 0, 0, 0},
};

void help(void) {
	puts("sync [OPTIONS] [FILES]...");
	puts("flush pending modifications to disk");
	puts("if no files are specified, flush the whole system");
	puts("-d --data        flush only data (not metadata)");
	puts("-f --file-system flush the whole filesystem containing the files");
}

int main(int argc, char **argv) {
	int opt;
	int option_index;
	while ((opt = getopt_long(argc, argv, "dfh", options, &option_index)) != -1) {
		switch (opt) {
		case 'd':
			data = 1;
			break;
		case 'f':
			file_system = 1;
			break;
		case 'h':
			help();
			return 0;
		case '?':
			return 1;
		}
	}

	if (optind == argc) {
		sync();
		return 0;
	}

	int status = 0;
	for (int i = optind; i < argc; i++) {
		int fd = open(argv[i], O_RDONLY);
		if (fd < 0) {
error:
			fprintf(stderr, "sync : %s : %m\n", argv[i]);
			status = 1;
			continue;
		}
		int ret = 0;
		if (file_system) {
			ret = syncfs(fd);
		} else if (data) {
			ret = fdatasync(fd);
		} else {
			ret = fsync(fd);
		}
		close(fd);
		if (ret < 0) goto error;
	}
	return status;
}

