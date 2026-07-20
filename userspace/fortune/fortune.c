#include <stdio.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>

typedef struct string {
	off_t offset;
	size_t length;
} string_t;

string_t *strings = NULL;
size_t strings_count;
char **files;
size_t files_count;

struct option options[] = {
	{"help",        no_argument,       NULL, 'h'},
	{0, 0, 0, 0},
};

void help(void) {
	puts("fortune [OPTIONS] [FILES]");
}

void add_file(const char *file) {
	files = realloc(files, sizeof(char *) * (files_count + 1));
	if (!files) {
		fprintf(stderr, "fortune : realloc : %m\n");
		exit(1);
	}
	files[files_count] = strdup(file);
	files_count++;
}

void add_directory(const char *dirname) {
	DIR *dir = opendir(dirname);
	if (!dir) return;
	struct dirent *entry;
	while ((entry = readdir(dir))) {
		if (entry->d_type != DT_REG) continue;
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s/%s", dirname, entry->d_name);
		add_file(path);
	}
	closedir(dir);
}

void add_string(off_t offset, size_t length) {
	strings = realloc(strings, sizeof(string_t) * (strings_count + 1));
	if (!strings) {
		fprintf(stderr, "fortune : realloc : %m\n");
		exit(1);
	}
	strings[strings_count].offset = offset;
	strings[strings_count].length = length;
	strings_count++;
}

static void process_file(const char *pathname, FILE *in) {
	char line[1024];
	size_t last_off = 0;
	char *ptr;
	do {
		ptr = fgets(line, sizeof(line), in);
		if (!ptr || (line[0] == '%' && line[1] == '\n')) {
			off_t off = ftell(in);
			size_t len = off - last_off;
			if (ptr) {
				len -= strlen(line);
			}
			add_string(last_off, len);
			last_off = off;
		}
	} while (ptr != NULL);
	if (ferror(in)) {
		fprintf(stderr, "fortune : %s : %m\n", pathname);
		exit(1);
	}
}

int main(int argc, char **argv) {
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	srand(ts.tv_sec * 10000000000 + ts.tv_nsec);

	int opt_index;
	int opt;
	while ((opt = getopt_long(argc, argv, "h", options, &opt_index)) != -1) {
		switch (opt) {
		case 'h':
			help();
			return 0;
		case '?':
			return 1;
		}
	}

	if (optind < argc) {
		for (int i=optind; i<argc; i++) {
			add_file(argv[i]);
		}
	} else {
		// TODO : search in fortune path
		const char *fortune_path = getenv("FORTUNE_PATH");
		if (!fortune_path) {
			fortune_path = PREFIX"/share/games/fortunes:/usr/local/share/games/fortunes";
		}
		char *path = strdup(fortune_path);
		char *tok = strtok(path, ":");
		while (tok) {
			add_directory(tok);
			tok = strtok(NULL, ":");
		}

	}
	if (files_count == 0) return 0;
	const char *file = files[rand() % files_count];

	FILE *in = fopen(file, "r");
	if (!in) {
		fprintf(stderr, "fortune : %s : %m\n", argv[optind]);
		return 1;
	}

	process_file(argv[optind], in);

	size_t i = rand() % (strings_count);
	size_t remain = strings[i].length;
	fseek(in, strings[i].offset, SEEK_SET);
	while (remain > 0) {
		char buf[4096];
		size_t count = remain > sizeof(buf) ? sizeof(buf) : remain;
		fread(buf, count, 1, in);
		fwrite(buf, count, 1, stdout);
		remain -= count;
	}

	return 0;
}
