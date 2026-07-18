#include <libini.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#ifdef __x86_64__
const char *arch = "x86_64";
#elif defined(__i386__)
const char *arch = "i386";
#elif defined(__aarch64__)
const char *arch = "aarch64";
#else
const char *arch = "unknown-arch"
#endif

#ifdef __linux__
const char *kernel = "Linux";
const char *os     = "GNU/Linux";
#elif defined(__stanix__)
const char *kernel = "Stanix";
const char *os     = "Stanix";
#elif defined(__unix__)
const char *kernel = "unix";
const char *os     = "unix";
#else
const char *kernel = "unknown-kernel";
const char *os     = "unknown-os";
#endif

void help(void) {
	puts("uname [OPTION]");
	puts("-a --all              : print all information in the following order");
	puts("-s --kernel-name      : print kernel name");
	puts("-r --kernel-release   : print kernel release");
	puts("-v --kernel-version   : print kernel version");
	puts("-m --machine          : print machine hardware name");
	puts("-o --operating-system : print operating system name");
}

struct option options[] = {
	{"all",              no_argument, NULL, 'a'},
	{"kernel-name",      no_argument, NULL, 's'},
	{"kernel-release",   no_argument, NULL, 'r'},
	{"kernel-version",   no_argument, NULL, 'v'},
	{"machine",          no_argument, NULL, 'm'},
	{"operating-system", no_argument, NULL, 'o'},
	{"help",             no_argument, NULL, 'h'},
	{0, 0, 0, 0},
};

int main(int argc, char **argv) {
	int show_kernel_name    = 0;
	int show_kernel_release = 0;
	int show_kernel_version = 0;
	int show_arch           = 0;
	int show_os             = 0;
	int opt;
	int opt_index;
	opterr = 0;
	while ((opt = getopt_long(argc, argv, "asrvmoh", options, &opt_index)) != -1) {
		switch (opt) {
		case 'a':
			show_kernel_name    = 1;
			show_kernel_release = 1;
			show_kernel_version = 1;
			show_arch           = 1;
			show_os             = 1;
			break;
		case 's':
			show_kernel_name = 1;
			break;
		case 'r':
			show_kernel_release = 1;
			break;
		case 'v':
			show_kernel_version = 1;
			break;
		case 'm':
			show_arch = 1;
			break;
		case 'o':
			show_os = 1;
			break;
		case 'h':
			help();
			return 0;
		case '?':
			if (optopt) {
				fprintf(stderr, "uname : invalid option '-%c'\n", optopt);
			} else {
				fprintf(stderr, "uname : invalid option '%s'\n", argv[optind-1]);
			}
			return 1;
		}

	}

	if (optind < argc) {
		fprintf(stderr, "uname : too many arguments\n");
		return 1;
	}

	// by default show kernel name only
	if (!(show_kernel_name || show_kernel_release || show_kernel_version || show_arch || show_os)) {
		show_kernel_name = 1;
	}

	utils_shashmap_t *infos = ini_parse_file("/etc/os-release");

	if (show_kernel_name) printf("%s ", kernel);
	if (show_kernel_release) printf("%s ", (char *)utils_shashmap_get(infos, "VERSION_ID"));
	if (show_kernel_version) printf("%s ", (char *)utils_shashmap_get(infos, "VERSION"));
	if (show_arch) printf("%s ", arch);
	if (show_os) printf("%s ", os);
	putchar('\n');
	return 0;
}
