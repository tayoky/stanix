#include <stdio.h>
#include <stdlib.h>
#include <libutils/hashmap.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>

// tty session manager

typedef struct session {
	pid_t pid;
	char *path;
} session_t;

utils_hashmap_t sessions;

void remove_session(session_t *session) {
	syslog(LOG_INFO, "terminate session '%s'", session->path);
	utils_hashmap_remove(&sessions, session->pid);
	free(session->path);
	free(session);
}

session_t *get_session(pid_t pid) {
	return utils_hashmap_get(&sessions, pid);
}

void start_getty(session_t *session) {
	utils_hashmap_remove(&sessions, session->pid);
	pid_t child = fork();
	if (child) {
		session->pid = child;
		utils_hashmap_add(&sessions, session->pid, session);
		return;
	}
	execl("/bin/getty", "getty", session->path, NULL);
	exit(1);
}

session_t *add_session(const char *path) {
	session_t *session = malloc(sizeof(session_t));
	if (!session) return NULL;
	session->pid = 0;
	session->path = strdup(path);
	syslog(LOG_INFO, "start session '%s'", session->path);
	start_getty(session);
	return NULL;
}

void try_path(const char *prefix) {
	for (int i=0; i<9; i++) {
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "/dev/%s%d", prefix, i);
		struct stat buf;
		if (stat(path, &buf) < 0) continue;
		add_session(path);
	}
}

int main() {
	utils_init_hashmap(&sessions, 16);
	try_path("ttyS");

	openlog("logind", LOG_CONS | LOG_PID, LOG_AUTH);
	pid_t pid;
	while ((pid = waitpid(-1, NULL, 0)) != -1) {
		session_t *session = get_session(pid);
		if (!session) continue;

		struct stat buf;
		if (stat(session->path, &buf) < 0) {
			// tty closed
			remove_session(session);
			continue;
		}
		start_getty(session);
	}

	// no session
	// our job is finished
	return 0;
}
