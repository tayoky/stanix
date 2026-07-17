#include <tserv-internal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

// handle clients

static utils_vector_t clients;
static int server_socket;

void init_clients(void) {
	utils_init_vector(&clients, sizeof(client_t));
	server_socket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
	
	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
		.sun_path = "/tmp/tserv",
	};
	bind(server_socket, (struct sockaddr *)&addr, sizeof(addr));
	listen(server_socket, 5);

	add_poll_fd(server_socket, POLLIN);
}

static void handle_connection(void) {
	int client_socket = accept(server_socket, NULL, NULL);
	if (client_socket < 0) return;

	client_t client = {
		.sock = client_socket,
	};
	utils_vector_push_back(&clients, &client);
	add_poll_fd(client_socket, POLLIN | POLLHUP);
}

static void kick_client(client_t *client) {
	remove_poll_fd(client->sock);
	close(client->sock);
	
	client_t *last = utils_vector_at(&clients, clients.count - 1);
	if (client != last) {
		memcpy(client, last, sizeof(client_t));
	}
	utils_vector_pop_back(&clients, NULL);
}

static size_t get_request_size(tserv_request_type_t request_type) {
	switch (request_type) {
	case TSERV_REQUEST_SET_RUNLEVEL:
		return sizeof(tserv_request_set_runlevel_t);
	case TSERV_REQUEST_GET_RUNLEVEL:
		return sizeof(tserv_request_get_runlevel_t);
	case TSERV_REQUEST_GET_RUNLEVEL_INFO:
		return sizeof(tserv_request_get_runlevel_info_t);
	case TSERV_REQUEST_START_SERVICE:
		return sizeof(tserv_request_start_service_t);
	case TSERV_REQUEST_END_SERVICE:
		return sizeof(tserv_request_end_service_t);
	case TSERV_REQUEST_STOP_SERVICE:
		return sizeof(tserv_request_stop_service_t);
	case TSERV_REQUEST_CONTINUE_SERVICE:
		return sizeof(tserv_request_continue_service_t);
	case TSERV_REQUEST_GET_SERVICE_STATUS:
		return sizeof(tserv_request_get_service_status_t);
	case TSERV_REQUEST_RELOAD_CONF:
		return sizeof(tserv_request_reload_conf_t);
	default:
		return sizeof(tserv_request_t);
	}
}

static void send_response(client_t *client, void *data, size_t size) {
	send(client->sock, data, size, 0);
}

static void send_success(client_t *client, tserv_request_id_t request_id) {
	tserv_response_success_t response = {
		.response_type = TSERV_RESPONSE_SUCCESS,
		.request_id = request_id,
	};
	send_response(client, &response, sizeof(response));
}

static void send_error(client_t *client, tserv_request_id_t request_id, int error) {
	tserv_response_error_t response = {
		.response_type = TSERV_RESPONSE_ERROR,
		.request_id = request_id,
		.error = error,
	};
	send_response(client, &response, sizeof(response));
}

static void handle_request(client_t *client, tserv_request_t *request) {
	switch (request->request_type) {
	// TODO : handle requests
	default:
		send_error(client, request->request_id, TSERV_ERROR_INVALID_REQUEST);
		break;
	}
}

static int handle_client(client_t *client) {
	tserv_request_t request;
	ssize_t r = read(client->sock, &request, sizeof(request));
	if ((r < 0 && errno != EAGAIN) || r < sizeof(request)) {
		kick_client(client);
		return 1;
	}

	size_t total_size = get_request_size(request.request_type);
	size_t remaining = total_size - sizeof(request);
	char *data = malloc(total_size);
	memcpy(data, &request, sizeof(request));
	read(client->sock, data + sizeof(request), remaining);

	handle_request(client, (tserv_request_t*)data);
	free(data);
	return 0;
}

void handle_clients(void) {
	handle_connection();
	utils_vector_foreach (&clients, client) {
		if (handle_client(client)) break;
	}
}

