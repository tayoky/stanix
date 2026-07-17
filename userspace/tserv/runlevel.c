#include <tserv-internal.h>

runlevel_t *get_runlevel(const char *name) {
	return utils_shashmap_get(&dep_tree->runlevels, name);
}

static int run_runlevel(runlevel_t *runlevel) {
	if (runlevel->stop_all) {
		// we need to stop all services first
		utils_shashmap_foreach (name, service, &dep_tree->services) {
			(void)name;
			int ret = service_stop(service);
			if (ret < 0 && ret != TSERV_ERROR_NOT_RUNNING) return ret;
		}
	}
	utils_vector_foreach (&runlevel->require, name) {
		runlevel_t *required = get_runlevel(name);
		// TODO : correct error
		if (!required) return -TSERV_ERROR_UNKNOWN;
		int ret = run_runlevel(required);
		if (ret < 0) return ret;
	}

	// launch services we need to launch
	utils_vector_foreach (&runlevel->services, name) {
		service_t *service = get_service(name);
		// TODO : error handling
		if (!service) return -TSERV_ERROR_UNKNOWN;

		int ret = service_start(service);
		if (ret < 0 && ret != TSERV_ERROR_ALREADY_RUNNING) return ret;
	}
	return 0;
}

int set_runlevel(runlevel_t *runlevel) {
	if (dep_tree->runlevel == runlevel) {
		// already set
		return 0;
	}
	
	int ret = run_runlevel(runlevel);
	if (ret < 0) return ret;

	dep_tree->runlevel = runlevel;
	return 0;
}
