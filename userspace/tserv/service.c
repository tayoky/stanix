#include <tserv-internal.h>

static void service_set_state(service_t *service, int state) {
	service->state = state;
	clock_gettime(CLOCK_REALTIME, &service->last_change);
}

int service_start(service_t *service) {
	if (service->state != TSERV_SERVICE_STATE_INACTIVE && service->state != TSERV_SERVICE_STATE_FAILED) {
		// already started
		return 0;
	}
	utils_vector_foreach (&service->conflict, name) {
		service_t *service = service_get(name);
		if (!service) continue;
		int ret = service_stop(service);
		if (ret < 0 && ret != TSERV_ERROR_NOT_RUNNING) return 0;
	}

	utils_vector_foreach (&service->require, name) {
		service_t *service = service_get(name);
		if (!service) {
			// TODO : error
			return -TSERV_ERROR_UNKNOW;
		}
		int ret = service_start(service);
		if (ret < 0) return ret;
	}

	if (service->action) {
		// the service is builtin action
		if (!strcmp(service->action, "shutdown")) {
		} else if (!strcmp(service->action, "reboot")) {
		}
	}

	pid_t pid = fork();
	if (pid < 0) {
		return -TSERV_ERROR_FORK_FAILED;
	}
	if (!pid) {
		// child
		// TODO : execute command
		return 0;
	}

	tservice_set_state(service, TSERV_SERVICE_STATE_RUNNING);
}

int service_end(service_t *service) {
	if (service->state != TSERV_STATE_RUNNING && service->state != TSERV_STATE_STOPED) {
		return -TSERV_ERROR_NOT_RUNNING;
	}
	kill(service->pid, SIGTERM);
	// TODO : send SIGKILL after some time
	service_set_state(service, TSERV_SERVICE_STATE_ENDING);
	return 0;
}

int service_stoping(service_t *service) {
	if (service->state != TSERV_STATE_RUNNING) {
		return -TSERV_ERROR_NOT_RUNNING;
	}
	kill(service->pid, SIGSTOP);
	service_set_state(service, TSERV_STATE_STOPING);
	return 0;
}

int handle_service(void) {
	int status;
	pid_t pid = waitpid(-1, &status, WNOHANG);
	if (pid < 0) return 0;

	// TODO : mark process as stoped
	service_t *service = get_service_from_pid(pid);

	if (WIFSTOPED(status)) {
	} else if (WIFSIGNALED(status)) {
	} else {

	}
	return 1;
}

void handle_services(void) {
	while (handle_service());
}
