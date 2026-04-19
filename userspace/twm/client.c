#include <twm-internal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <twm.h>

utils_vector_t clients;
int desktop_hook;

static int close_client_window(client_t *client) {
	utils_hashmap_foreach(key, element, &windows) {
		(void)key;
		window_t *window = element;
		if (window->client == client->id) {
			destroy_window(window);
			return 1;
		}
	}
	return 0;
}

void kick_client(client_t *client) {
	if (desktop_hook == client) desktop_hook = NULL;
	
	// close all windows of this client
	while (close_client_window(client));
	close(client->fd);
	
	client_t *last_client = utils_vector_at(&clients, clients.count-1);
	if (last_client != client) {
		memcpy(client, last_client, sizeof(client_t));
	}
	utils_vector_pop_back(&clients, NULL);
}

int accept_client(void) {
	static int client_id = 1;
	int client_fd = accept(server_socket, NULL, NULL);
	if (client_fd < 0) {
		error("fail to accept connection");
		return -1;
	}
	puts("client connected");
	client_t client = {
		.fd = client_fd,
		.id = client_id++,
	};
	utils_vector_push_back(&clients, &client);
	return 0;
}

client_t *get_client(int id) {
	for (size_t i=0; i<clients.count; i++) {
		client_t *client = utils_vector_at(&clients, i);
		if (client->id == id) return client;
	}
	return NULL;
}
