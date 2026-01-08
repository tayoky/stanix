#include <twm-internal.h>
#include <sys/socket.h>
#include <stdio.h>
#include <twm.h>

int send_event(client_t *client, twm_event_t *event) {
	return send(client->fd, event, event->size, 0);
}

static void handle_create_window(client_t *client, twm_request_create_window_t *request) {
	puts("create window");
	static twm_window_t id_count = 0;
	window_t *window = malloc(sizeof(window_t));
	memset(window, 0, sizeof(window_t));

	window->id     = id_count++;
	window->client = client;
	window->width  = request->width;
	window->height = request->height;
	window->x = 100;
	window->y = 100;
	window->title  = strnlen(request->title, sizeof(request->title)) < sizeof(request->title) ? strdup(request->title) : strdup("window");

	utils_hashmap_add(&windows, window->id, window);
	twm_event_window_created_t event = {
		.base = {
			.request_id = request->base.id,
			.size       = sizeof(event),
		},
		.id = window->id,
	};
	send_event(client, (twm_event_t*)&event);
	render_window_decor(window);
	render_window_content(window);
	return;
}

static void handle_init(client_t *client, twm_request_init_t *request) {
	if (request->major != TWM_CURRENT_MAJOR || request->minor != TWM_CURRENT_MINOR) {
		kick_client(client);
	}
}

int handle_request(client_t *client){
	char buf[TWM_MAX_PACKET_SIZE];
	twm_request_t *request = (twm_request_t*)buf;
	ssize_t size = recv(client->fd, buf, sizeof(twm_request_t), 0);
	if (size < (ssize_t)sizeof(twm_request_t)) return -1;
	if (request->size > TWM_MAX_PACKET_SIZE) return -1;
	
	size = recv(client->fd, buf + sizeof(twm_request_t), request->size - sizeof(twm_request_t), 0);
	if (size < 0) return -1;

	switch (request->type) {
	case TWM_REQUEST_CREATE_WINDOW:
		handle_create_window(client, (twm_request_create_window_t*)request);
		break;
	case TWM_REQUEST_INIT:
		handle_init(client, (twm_request_init_t*)request);
		break;
	}
	
	return 0;
}
