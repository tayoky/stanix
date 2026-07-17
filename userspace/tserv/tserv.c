#include <tserv.h>
#include <tserv-internal.h>
#include <poll.h>

static utils_vector_t poll_fds;

static void init_pollfd(void) {
	utils_init_vector(&poll_fds, sizeof(struct pollfd));
}

int add_poll_fd(int fd, int events) {
	struct pollfd poll_fd = {
		.fd = fd,
		.events = events,
	};
	utils_vector_push_back(&poll_fds, &poll_fd);
	return 0;
}

void remove_poll_fd(int fd) {
	// TODO
}

int main() {
	init_pollfd();
	init_clients();
	init_signal();
	dep_tree = generate_dep_tree();

	// TODO : parse cmdline to find run leve
	runlevel_t *default_runlevel = get_runlevel("default");
	set_runlevel(default_runlevel);
	
	for (;;) {
		poll(poll_fds.data, poll_fds.count, -1);
		handle_clients();
		handle_signal();
	}
}
