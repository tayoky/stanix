#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <twm.h>

twm_ctx_t ctx;


int twm_init(const char *path) {
    if (!path) path = getenv("TWM");
    if (!path) path = getenv("DISPLAY");
    if (!path) return -1;

    ctx.fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ctx.fd < 0) return -1;

    struct sockaddr_un address = {
        .sun_family = AF_UNIX,
    };
    strncpy(address.sun_path, path, sizeof(address.sun_path));

    if (connect(ctx.fd, (struct sockaddr*)&address, sizeof(address)) < 0) goto close_socket;

    twm_request_init_t init_request = {
        .base = {
            .id = TWM_REQUEST_INIT,
            .size = sizeof(init_request),
        },
        .major = TWM_CURRENT_MAJOR,
        .minor = TWM_CURRENT_MINOR,
    };
    if (twm_send_request((twm_request_t*)&init_request) < 0) goto close_socket;

    return 0;

close_socket:
    close(ctx.fd);
    return -1;
}