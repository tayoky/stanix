#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <common.h>

static long parse_long(const char *str, size_t line) {
	char *end;
	long l = strtol(str, &end, 0);
	if (end == str || *end) {
		error("pw.conf : invalid integer '%s' at line %zu", str, line);
		exit(1);
	}
	return l;
}

void load_conf(void) {
	char conf_path[PATH_MAX];
	snprintf(conf_path, sizeof(conf_path), "%s%s/etc/pw.conf", root, prefix);
	FILE *conf = fopen(conf_path, "r");
	if (!conf) {
		return;
	}

	size_t i = 0;
	char line[1024];
	while (fgets(line, sizeof(line), conf)) {
		i++;
		char *key = strtok(line, " \n\r\t");
		if (!key || *key == '#') {
			continue;
		}
		char *value = strtok(NULL, " \n\r\t");
		if (!value) {
			error("pw.conf : no value on line %zu\n", i);
			exit(1);
		}
		if (!strcmp(key, "defaultshell")) {
			if (!shell) shell = strdup(value);
		} else if (!strcmp(key, "home")) {
			if (!home) {
				char default_home[PATH_MAX];
				snprintf(default_home, sizeof(default_home), "%s/%s", value, name);
				home = strdup(default_home);
			}
		} else if (!strcmp(key, "homemode")) {
			home_mode = parse_long(value, i);
		} else if (!strcmp(key, "minuid")) {
			min_uid = parse_long(value, i);
		} else if (!strcmp(key, "maxuid")) {
			max_uid = parse_long(value, i);
		}
		// TODO : parse more
	}
	fclose(conf);

	// defaults
	if (!shell) {
		shell = "/bin/sh";
	}
	if (!home) {
		char default_home[PATH_MAX];
		snprintf(default_home, sizeof(default_home), "/home/%s", name);
		home = strdup(default_home);
	}
	if (!password) {
		password = "";
	}
	if (!class) {
		class = "default";
	}
	if (!comment) {
		comment = "";
	}
}
