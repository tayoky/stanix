#include <sys/mount.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


struct option options[] = {
	{"type",    required_argument, NULL, 't'},
	{"target",  required_argument, NULL, 'T'},
	{"source",  required_argument, NULL, 'S'},
	{"options", required_argument, NULL, 'o'},
	{"move",    no_argument      , NULL, 'M'},
	{"help",    no_argument      , NULL, 'h'},
	{0, 0, 0, 0},
};

void help(){
	printf("mount -t TYPE [OPTIONS] [-s] SOURCE [-T] TARGET\n");
	printf("-t/--type    : precise type\n");
	printf("-S/--source  : precise source/device to mount (can be a stub for tmpfs)\n");
	printf("-T/--target  : path to mount to\n");
	printf("-o/--options : options to mount with separated by comma (nodev/noexec/nosuid/rdonly/ro)\n");
	printf("-M/--move    : move a move point from SOURCE to TARGET\n");
}

int main(int argc,char **argv){
	char *type = NULL;
	char *source = NULL;
	char *target = NULL;
	unsigned long flags = 0;

	int opt;
	while ((opt = getopt_long(argc, argv, "t:T:S:o:Mh", options, NULL)) != -1) {
		switch (opt) {
		case 't':
			type = optarg;
			break;
		case 'T':
			target = optarg;
			break;
		case 'S':
			source = optarg;
			break;
		case 'o':;
			char *cur = strtok(optarg, ",");
			while (cur) {
				if (!strcmp(cur, "nodev")) {
					flags |= MS_NODEV;
				} else if (!strcmp(cur, "noexec")) {
					flags |= MS_NOEXEC;
				} else if (!strcmp(cur, "nosuid")) {
					flags |= MS_NOSUID;
				} else if (!strcmp(cur, "rdonly") || !strcmp(cur, "ro")) {
					flags |= MS_RDONLY;
				} else {
					fprintf(stderr, "mount : unknown option '%s'\n", cur);
					return 1;
				}
				cur = strtok(NULL, ",");
			}
			break;
		case 'M':
			flags |= MS_MOVE;
			break;
		case 'h':
			help();
			return 0;
		case '?':
			return 1;
		}
	}
	
	if (!source && optind < argc) {
		source = argv[optind++];
	}
	if (!target && optind < argc) {
		target = argv[optind++];
	}

	if (!type) {
		if (flags & MS_MOVE) {
			type = "";
		} else {
			fprintf(stderr, "mount : no type specified\n");
			return 1;
		}
	}
	if (!source) {
		fprintf(stderr, "mount : no source specified\n");
		return 1;
	}
	if (!target) {
		fprintf(stderr, "mount : no target specified\n");
		return 1;
	}

	printf("mount : mounting the device %s under %s , type : %s\n", source, target, type);

	int ret = mount(source, target, type, flags, NULL);
	if (ret < 0) {
		perror("mount");
		return 1;
	}
	return 0;
}
