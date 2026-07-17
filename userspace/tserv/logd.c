#include <sys/socket.h>
#include <sys/un.h>
#include <tlogd.h>
#include <poll.h>
#include <stdio.h>

// logging deamon

typedef struct msg {
	utils_list_node_t node;
	char *hostname;
	char *app;
	int pri;
	int version;
	time_t timestamp;
	pid_t pid;
	char *msgid;
	char *msg;
} msg_t;

void handle_packet(int sock) {
	// TODO : handle RFC 3164
	// TODO : handle RFC 5424
}

int server_socket;
int main() {
	server_socket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (server_socket < 0) {
		perror("socket");
		return 1;
	}

	struct sockaddr_un unix_path = {
		.sun_family = AF_UNIX,
		.sun_path = "/dev/log",
	};
	if (bind(server_socket, (struct sockaddr *)unix_path, sizeof(unix_path)) < 0) {
		perror("bind");
		return 1;
	}

	if (listen(server_socket, 5) < 0) {
		perror("listen");
		return 1;
	}

	for (;;) {

	}

	return 0;
}
