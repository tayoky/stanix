#include <tserv-internal.h>
#include <libini.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>
#include <stdio.h>

dep_tree_t *dep_tree;

static char *safe_strdup(const char *str) {
	if (!str) return NULL;
	return strdup(str);
}

static char *parse_string_list(utils_vector_t *vector, utils_shashmap_t *data, const char *name) {
	utils_init_vector(vector, sizeof(char *));
	char *str = safe_strdup(utils_shashmap_get(data, name));
	if (str) {
		int prev_is_space = 1;
		for (char *ptr=str; *ptr; ptr++) {
			if (isblank(*ptr)) {
				prev_is_space = 1;
				*ptr = '\0';
			} else if (prev_is_space) {
				prev_is_space = 0;
				utils_vector_push_back(vector, (char*[]){ptr});
			}
		}
	}
	return str;
}

static int parse_services(dep_tree_t *dep_tree) {
	DIR *dir = opendir("/etc/tserv.d");
	if (!dir) return -1;
	struct dirent *entry;
	while ((entry = readdir(dir))) {
		if (entry->d_type != DT_REG) continue;
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "/etc/tserv.d/%s", entry->d_name);
		utils_shashmap_t *data = ini_parse_file(path);
		// TODO : error handling
		service_t *service = malloc(sizeof(service_t));
		memset(service, 0, sizeof(service_t));
		service->name = strdup(entry->d_name);
		if (strlen(service->name) > 4 && !strcmp(&service->name[strlen(service->name)-4], ".ini")) {
			service->name[strlen(service->name)-4] = '\0';
		}
		service->action = safe_strdup(utils_shashmap_get(data, "action"));
		service->after_buf = parse_string_list(&service->after, data, "after");
		service->before_buf = parse_string_list(&service->before, data, "before");
		service->require_buf = parse_string_list(&service->require, data, "require");
		service->conflicts_buf = parse_string_list(&service->conflicts, data, "conflicts");

		// command need to be NULL terminated
		service->command_buf = parse_string_list(&service->command, data, "command");
		utils_vector_push_back(&service->command, (char*[]){NULL});
		ini_free(data);
		utils_shashmap_add(&dep_tree->services, service->name, service);
	}
	closedir(dir);
	return 0;
}

static int parse_runlevels(dep_tree_t *dep_tree) {
	DIR *dir = opendir("/etc/tserv.d/runlevels");
	if (!dir) return -1;
	struct dirent *entry;
	while ((entry = readdir(dir))) {
		if (entry->d_type != DT_REG) continue;
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "/etc/tserv.d/%s", entry->d_name);
		utils_shashmap_t *data = ini_parse_file(path);
		// TODO : error handling
		runlevel_t *runlevel = malloc(sizeof(runlevel_t));
		memset(runlevel, 0, sizeof(runlevel_t));
		runlevel->name = strdup(entry->d_name);
		if (strlen(runlevel->name) > 4 && !strcmp(&runlevel->name[strlen(runlevel->name)-4], ".ini")) {
			runlevel->name[strlen(runlevel->name)-4] = '\0';
		}

		runlevel->services_buf = parse_string_list(&runlevel->services, data, "services");
		runlevel->require_buf  = parse_string_list(&runlevel->require, data, "require");
		runlevel->conflicts_buf = parse_string_list(&runlevel->conflicts, data, "conflicts");
		
		ini_free(data);

	}
	closedir(dir);
	return 0;
}

dep_tree_t *generate_dep_tree(void) {
	dep_tree_t *dep_tree = malloc(sizeof(dep_tree_t));
	if (!dep_tree) return NULL;
	memset(dep_tree, 0, sizeof(dep_tree_t));
	utils_init_shashmap(&dep_tree->services, 32);
	utils_init_shashmap(&dep_tree->runlevels, 8);

	if (parse_services(dep_tree) < 0) goto error;
	if (parse_runlevels(dep_tree) < 0) goto error;
	return dep_tree;

error:
	free_dep_tree(dep_tree);
	return NULL;
}

void free_dep_tree(dep_tree_t *dep_tree) {
	// TODO : free more stuff
	utils_free_shashmap(&dep_tree->services);
	utils_free_shashmap(&dep_tree->runlevels);
	free(dep_tree);
}	
