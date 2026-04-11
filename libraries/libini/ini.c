#include <libini.h>
#include <stdlib.h>
#include <string.h>
#include <libutils/shashmap.h>
#include <stdio.h>
#include <ctype.h>

static char *skip_blank(char *ptr) {
	while (isblank(*ptr)) ptr++;
	return ptr;
}

static int is_end(int c) {
	return c == '#' || c == '\n' || c == '\r' || c == '\0';
}

utils_shashmap_t *ini_parse_file(const char *filename) {
	FILE *file = fopen(filename, "r");
	if (!file) return NULL;

	utils_shashmap_t *hashmap = malloc(sizeof(utils_shashmap_t));
	if (!hashmap) return NULL;
	memset(hashmap, 0, sizeof(utils_shashmap_t));
	utils_init_shashmap(hashmap, 64);

	char line[LINE_MAX];
	char section[LINE_MAX] = {'\0'};
	while (fgets(line, sizeof(line), file)) {
		char *ptr = skip_blank(line);
		if (is_end(*ptr)) continue;

		if (*ptr == '[') {
			// section
			ptr++;
			char *end = ptr;
			while(*end != ']' && !is_end(*end)) end++;
			memcpy(section, ptr, end - ptr);
			section[ptr - end] = '\0';
		} else {
			char *equal = strchr(ptr, '=');
			*equal = '\0';
			char *raw_value = skip_blank(equal+1);
			char value[LINE_MAX];
			char *dest = value;
			while (!is_end(*raw_value)) {
				if (*raw_value != '"') {
					*(dest++) = *raw_value;
				}
				raw_value++;
			}
			*dest = '\0';

			char name[2 * LINE_MAX];
			if (section[0]) {
				sprintf(name, "%s.%s", section, ptr);
			} else {
				strcpy(name, ptr);
			}
			utils_shashmap_add(hashmap, name, value);
		}
	}

	return hashmap;
}
