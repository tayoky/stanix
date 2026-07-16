#ifndef LIBTSERV_H
#define LIBTSERV_H

#include "tserv.h"

typedef struct tserv_ctx tserv_ctx_t;

tserv_ctx_t *tserv_connect_path(const char *path);
tserv_ctx_t *tserv_connect(void);
void tserv_disconnect(tserv_ctx_t *ctx);
int tserv_set_runlevel(tserv_ctx_t *ctx, const char *runlevel);
int tserv_get_runlevel(tserv_ctx_t *ctx, char **runlevel);
int tserv_get_runlevel_info(tserv_ctx_t *ctx, const char *runlevel, tserv_runlevel_info_t *info);
int tserv_start_service(tserv_ctx_t *ctx, const char *service);
int tserv_end_service(tserv_ctx_t *ctx, const char *service);
int tserv_stop_service(tserv_ctx_t *ctx, const char *service);
int tserv_continue_service(tserv_ctx_t *ctx, const char *service);
int tserv_get_service_status(tserv_ctx_t *ctx, const char *service, tserv_service_status_t *status);
int tserv_reload_conf(tserv_ctx_t *ctx);

#endif
