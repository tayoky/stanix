#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <getopt.h>

struct option options[] = {
	{"list", no_argument, 0, 'l'},
	{0,0,0,0},
};

int server;
int client;
struct sockaddr_un server_addr = {
	.sun_family = AF_UNIX,
	.sun_path = "/tmp/test-unix.sock",
};

void *client_thread(void *arg){
	(void)arg;

	if (connect(client, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		perror("connected");
		return NULL;
	}

	puts("connected");

	return NULL;
}

int main(int argc, char **argv){
	server = socket(AF_UNIX, SOCK_STREAM, 0);
	client = socket(AF_UNIX, SOCK_STREAM, 0);

	if (server < 0 || client < 0) {
		perror("socket");
		return 1;
	}

	if (bind(server, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		perror("bind");
		return 1;
	}

	if (listen(server, 5) < 0) {
		perror("listen");
		return 1;
	}

	if (getopt_long(argc, argv, "l", options, NULL) == 'l') {
		system("ls -l /tmp");
	}

	pthread_t thread;
	pthread_create(&thread, NULL, client_thread, NULL);

	int connected_sock;
	if ((connected_sock = accept(server, NULL, NULL)) < 0) {
		perror("accept");
		return 1;
	}

	pthread_join(thread, NULL);

	close(connected_sock);
	close(server);
	close(client);
	return 0;
}
