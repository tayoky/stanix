#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <twm.h>

extern twm_ctx_t ctx;


int twm_send_request(twm_request_t *request) {
    return send(ctx.fd, request, request->size, 0);
}


twm_window_t twm_create_window(const char *title, long width, long height) {
    twm_request_create_window_t request = {
        .base = {
            .type = TWM_REQUEST_CREATE_WINDOW,
            .size = sizeof(request),
        },
        .width  = width,
        .height = height,
    };
    strncpy(request.title, title, sizeof(request.title) - 1);

    // TODO : wait for event
    return twm_send_request((twm_request_t*)&request);
}
