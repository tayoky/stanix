#include <tserv-internal.h>
#include <libini.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>

static char *safe_strdup(const char *str) {
	if (!str) return NULL;
	return strdup(str);
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
		service->name = strdup(entry->d_name);
		if (strlen(service->name) > 4 && !strcmp(&service->name[strlen(service->name)-4], ".ini")) {
			service->name[strlen(service->name)-4] = '\0';
		}
		service->action = safe_strdup(utils_shashmap_get(data, "action"));
		// TODO : parse other fields
		ini_free(data);
		utils_shashmap_add(&dep_tree->services, service->name, service);
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
