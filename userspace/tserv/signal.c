#include <tserv-internal.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

int signal_fd;
static int signal_fd_out;

static void sig_handler(int signum) {
	write(signal_fd_out, &signum, sizeof(signum));
}

static void make_nonblock(int fd) {
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

void init_signal(void) {
	int pipefd[2];
	pipe(pipefd);

	signal_fd = pipefd[0];
	signal_fd_out = pipefd[1];
	make_nonblock(signal_fd);
	make_nonblock(signal_fd_out);

	signal(SIGINT,  sig_handler);
	signal(SIGHUP,  sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGCHLD, sig_handler);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);

	add_poll_fd(signal_fd, POLLIN);
}

void handle_signal(void) {
	int signum = 0;
	if (read(signal_fd, &signum, sizeof(signum)) < 0) return;

	switch (signum) {
	case SIGCHLD:
		handle_services();
		break;
	default:
		// TODO : set runlevel
		break;
	}
}
