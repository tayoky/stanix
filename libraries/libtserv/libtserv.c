#include <libtserv.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

struct tserv_ctx {
	tserv_request_id_t ids_count;
	int sock;
};

tserv_ctx_t *tserv_connect_path(const char *path) {
	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) return NULL;

	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
	};
	strcpy(addr.sun_path, path);
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sock);
		return NULL;
	}

	tserv_ctx_t *ctx = malloc(sizeof(tserv_ctx_t));
	if (!ctx) {
		close(sock);
		return NULL;
	}
	ctx->sock = sock;
	ctx->ids_count = 1;
	return ctx;
}

tserv_ctx_t *tserv_connect(void) {
	return tserv_connect_path("/tmp/tserv");
}

void tserv_disconnect(tserv_ctx_t *ctx) {
	close(ctx->sock);
	free(ctx);
}

static size_t get_response_size(tserv_response_type_t type) {
	switch (type) {
	case TSERV_RESPONSE_SUCCESS:
		return sizeof(tserv_response_success_t);
	case TSERV_RESPONSE_ERROR:
		return sizeof(tserv_response_error_t);
	case TSERV_RESPONSE_SERVICE_STATUS:
		return sizeof(tserv_response_service_status_t);
	default:
		return sizeof(tserv_response_t);
	}
}

static void *get_response(tserv_ctx_t *ctx, tserv_request_id_t request_id) {
	tserv_response_t response;
	if (read(ctx->sock, &response, sizeof(response)) < (ssize_t)sizeof(response)) return NULL;

	if (response.request_id != request_id) {
		// corrupted
		return NULL;
	}

	size_t total_size = get_response_size(response.response_type);
	char *data = malloc(total_size);
	if (!data) return NULL;

	size_t remaining_size = total_size - sizeof(response);
	memcpy(data, &response, sizeof(response));
	if (read(ctx->sock, data + sizeof(response), remaining_size) < (ssize_t)remaining_size) return NULL;

	return data;
}

static void *send_request(tserv_ctx_t *ctx, void *data, size_t size) {
	tserv_request_t *request = data;
	request->request_id = ctx->ids_count++;
	if (send(ctx->sock, data, size, 0) < (ssize_t)size) return NULL;
	tserv_request_id_t request_id = request->request_id;
	free(request);
	return get_response(ctx, request_id);
}

static int send_simple_request(tserv_ctx_t *ctx, void *data, size_t size) {
	tserv_response_t *response = send_request(ctx, data, size);
	if (!response) return -TSERV_ERROR_UNKNOWN;
	int ret = 0;
	if (response->response_type == TSERV_RESPONSE_ERROR) {
		tserv_response_error_t *error = (tserv_response_error_t*)response;
		ret = error->error;
	}
	free(response);
	return ret;
}

static void fill_string(tserv_string_t *tserv_string, const char *string) {
	tserv_string->length = strlen(string);
	memcpy(tserv_string->data, string, tserv_string->length);
}

int tserv_set_runlevel(tserv_ctx_t *ctx, const char *runlevel) {
	size_t request_size = sizeof(tserv_request_set_runlevel_t) + strlen(runlevel);
	tserv_request_set_runlevel_t *request = malloc(request_size);
	if (!request) return -TSERV_ERROR_OUT_OF_MEMORY;
	request->request_type = TSERV_REQUEST_SET_RUNLEVEL;
	fill_string(&request->runlevel, runlevel);
	return send_simple_request(ctx, request, request_size);
}

int tserv_get_runlevel(tserv_ctx_t *ctx, char **runlevel);
int tserv_get_runlevel_info(tserv_ctx_t *ctx, const char *runlevel, tserv_runlevel_info_t *info);

int tserv_start_service(tserv_ctx_t *ctx, const char *service) {
	size_t request_size = sizeof(tserv_request_start_service_t) + strlen(service);
	tserv_request_start_service_t *request = malloc(request_size);
	if (!request) return -TSERV_ERROR_OUT_OF_MEMORY;
	request->request_type = TSERV_REQUEST_START_SERVICE;
	fill_string(&request->service, service);
	return send_simple_request(ctx, request, request_size);
}

int tserv_end_service(tserv_ctx_t *ctx, const char *service) {
	size_t request_size = sizeof(tserv_request_end_service_t) + strlen(service);
	tserv_request_end_service_t *request = malloc(request_size);
	if (!request) return -TSERV_ERROR_OUT_OF_MEMORY;
	request->request_type = TSERV_REQUEST_END_SERVICE;
	fill_string(&request->service, service);
	return send_simple_request(ctx, request, request_size);
}

int tserv_stop_service(tserv_ctx_t *ctx, const char *service) {
	size_t request_size = sizeof(tserv_request_stop_service_t) + strlen(service);
	tserv_request_stop_service_t *request = malloc(request_size);
	if (!request) return -TSERV_ERROR_OUT_OF_MEMORY;
	request->request_type = TSERV_REQUEST_STOP_SERVICE;
	fill_string(&request->service, service);
	return send_simple_request(ctx, request, request_size);
}

int tserv_continue_service(tserv_ctx_t *ctx, const char *service) {
	size_t request_size = sizeof(tserv_request_continue_service_t) + strlen(service);
	tserv_request_continue_service_t *request = malloc(request_size);
	if (!request) return -TSERV_ERROR_OUT_OF_MEMORY;
	request->request_type = TSERV_REQUEST_CONTINUE_SERVICE;
	fill_string(&request->service, service);
	return send_simple_request(ctx, request, request_size);
}

int tserv_get_service_status(tserv_ctx_t *ctx, const char *service, tserv_service_status_t *status);

int tserv_reload_conf(tserv_ctx_t *ctx) {
	size_t request_size = sizeof(tserv_request_reload_conf_t);
	tserv_request_reload_conf_t *request = malloc(request_size);
	if (!request) return -TSERV_ERROR_OUT_OF_MEMORY;
	request->request_type = TSERV_REQUEST_RELOAD_CONF;
	return send_simple_request(ctx, request, request_size);
}
