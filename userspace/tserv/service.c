#include <tserv-internal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

static void service_set_state(service_t *service, int state) {
	service->state = state;
	clock_gettime(CLOCK_REALTIME, &service->last_change);
}

service_t *get_service(const char *name) {
	return utils_shashmap_get(&dep_tree->services, name);
}

service_t *get_service_from_pid(pid_t pid) {
	// TODO
	return NULL;
}

int service_start(service_t *service) {
	if (service->state != TSERV_SERVICE_STATE_INACTIVE && service->state != TSERV_SERVICE_STATE_CRASHED) {
		// already started
		return 0;
	}
	utils_vector_foreach (&service->conflicts, name) {
		service_t *service = get_service(name);
		if (!service) continue;
		int ret = service_stop(service);
		if (ret < 0 && ret != TSERV_ERROR_NOT_RUNNING) return ret;
	}

	utils_vector_foreach (&service->require, name) {
		service_t *service = get_service(name);
		if (!service) {
			// TODO : error
			return -TSERV_ERROR_UNKNOWN;
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
		// setup environement
		if (service->gid) setgid(service->gid);
		if (service->uid) setuid(service->uid);
		// TODO : execute command
		return 0;
	}

	service->pid = pid;
	service_set_state(service, TSERV_SERVICE_STATE_RUNNING);
}

int service_end(service_t *service) {
	if (service->state != TSERV_SERVICE_STATE_RUNNING && service->state != TSERV_SERVICE_STATE_STOPPED) {
		return -TSERV_ERROR_NOT_RUNNING;
	}
	kill(service->pid, SIGTERM);
	// TODO : send SIGKILL after some time
	service_set_state(service, TSERV_SERVICE_STATE_ENDING);
	return 0;
}

int service_stop(service_t *service) {
	if (service->state != TSERV_SERVICE_STATE_RUNNING) {
		return -TSERV_ERROR_NOT_RUNNING;
	}
	kill(service->pid, SIGSTOP);
	service_set_state(service, TSERV_SERVICE_STATE_STOPING);
	return 0;
}

int service_continue(service_t *service) {
	if (service->state != TSERV_SERVICE_STATE_STOPING && service->state != TSERV_SERVICE_STATE_STOPPED) {
		return -TSERV_ERROR_NOT_STOPPED;
	}
	
	kill(service->pid, SIGCONT);
	service_set_state(service, TSERV_SERVICE_STATE_RUNNING);
	return 0;
}

static int handle_service(void) {
	int status;
	pid_t pid = waitpid(-1, &status, WNOHANG);
	if (pid < 0) return 0;

	// TODO : mark process as stoped
	service_t *service = get_service_from_pid(pid);
	if (!service) return 1;

	if (WIFSTOPPED(status)) {
		service_set_state(service, TSERV_SERVICE_STATE_STOPPED);
	} else if (WIFSIGNALED(status)) {
		int signum = WTERMSIG(status);
		if (signum != SIGKILL && signum != SIGTERM) {
			service_set_state(service, TSERV_SERVICE_STATE_CRASHED);
		} else {
			service_set_state(service, TSERV_SERVICE_STATE_INACTIVE);
		}
	} else {
		service_set_state(service, TSERV_SERVICE_STATE_INACTIVE);
	}
	return 1;
}

void handle_services(void) {
	while (handle_service());
}
