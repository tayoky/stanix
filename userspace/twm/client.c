#include <twm-internal.h>
#include <unistd.h>
#include <string.h>
#include <twm.h>

utils_vector_t clients;

static int close_client_window(client_t *client) {
	utils_hashmap_foreach(key, element, &windows) {
		(void)key;
		window_t *window = element;
		if (window->client == client) {
			destroy_window(window);
			return 1;
		}
	}
	return 0;
}

void kick_client(client_t *client) {
	// close all windows of this client
	while (close_client_window(client));
	close(client->fd);
	
	client_t *last_client = utils_vector_at(&clients, clients.count-1);
	if (last_client != client) {
		memcpy(client, last_client, sizeof(client_t));
	}
	utils_vector_pop_back(&clients, NULL);
}