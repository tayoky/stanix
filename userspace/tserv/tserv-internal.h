#ifndef TSERV_INTERNAL_H
#define TSERV_INTERNAL_H

#include <sys/types.h>
#include <libutils/vector.h>
#include <libutils/shashmap.h>
#include <time.h>
#include <poll.h>
#include <tserv.h>

typedef struct client {
	int sock;
} client_t;

typedef struct buf {
	char *data;
	size_t len;
} buf_t;

typedef struct service {
	char *name;
	pid_t pid;
	uid_t uid;
	gid_t gid;
	int state;
	struct timespec last_change;
	char *action;
	utils_vector_t after;
	utils_vector_t before;
	utils_vector_t require;
	utils_vector_t conflicts;
	buf_t after_buf;
	buf_t before_buf;
	buf_t require_buf;
	buf_t conflicts_buf;
} service_t;

typedef struct runlevel {
	char *name;
	utils_vector_t services;
	utils_vector_t require;
	utils_vector_t conflict;
} runlevel_t;

typedef struct dep_tree {
	utils_shashmap_t services;
	utils_shashmap_t runlevels;
	runlevel_t *runlevel;
} dep_tree_t;

extern dep_tree_t *dep_tree;
extern int signal_fd;

dep_tree_t *parse_files(void);
void handle_services(void);
int service_start(service_t *service);
int service_end(service_t *service);
int service_stop(service_t *service);
int service_continue(service_t *service);
void init_clients(void);
void handle_clients(void);
void init_signal(void);
void handle_signal(void);
int add_poll_fd(int fd, int events);
void remove_poll_fd(int fd);
#endif
