#ifndef TSERV_H
#define TSERV_H

#include <stdint.h>

typedef uint16_t tserv_request_type_t;
typedef uint16_t tserv_response_type_t;
typedef uint16_t tserv_request_id_t;
typedef uint16_t tserv_error_t;

#define TSERV_ERROR_UNKNOWN         0x0
#define TSERV_ERROR_INVALID_REQUEST 0x1
#define TSERV_ERROR_NOT_FOUND       0x2
#define TSERV_ERROR_OUT_OF_MEMORY   0x3
#define TSERV_ERROR_NOT_RUNNING     0x4
#define TSERV_ERROR_FORK_FAILED     0x5
#define TSERV_ERROR_NOT_STOPPED     0x6
#define TSERV_ERROR_ALREADY_RUNNING 0x7

typedef int64_t  tserv_pid_t;
typedef int8_t   tserv_service_state_t;

#define TSERV_SERVICE_STATE_INACTIVE 0x1
#define TSERV_SERVICE_STATE_STARTING 0x2
#define TSERV_SERVICE_STATE_RUNNING  0x3
#define TSERV_SERVICE_STATE_STOPING  0x4
#define TSERV_SERVICE_STATE_STOPPED  0x4
#define TSERV_SERVICE_STATE_ENDING   0x5
#define TSERV_SERVICE_STATE_CRASHED  0x6

typedef struct tserv_string {
	uint32_t length;
	char data[];
} tserv_string_t;

typedef struct tserv_runlevel_info {
	int32_t stub; // TODO
} tserv_runlevel_info_t;

typedef struct tserv_service_status {
	tserv_pid_t pid;
	tserv_service_state_t state;
} tserv_service_status_t;

#define TSERV_REQUEST_SET_RUNLEVEL       0x1
#define TSERV_REQUEST_GET_RUNLEVEL       0x2
#define TSERV_REQUEST_GET_RUNLEVEL_INFO  0x3
#define TSERV_REQUEST_START_SERVICE      0x4
#define TSERV_REQUEST_END_SERVICE        0x5
#define TSERV_REQUEST_STOP_SERVICE       0x6
#define TSERV_REQUEST_CONTINUE_SERVICE   0x7
#define TSERV_REQUEST_GET_SERVICE_STATUS 0x8
#define TSERV_REQUEST_RELOAD_CONF        0x9

#define TSERV_RESPONSE_SUCCESS        0x1
#define TSERV_RESPONSE_ERROR          0x2
#define TSERV_RESPONSE_SERVICE_STATUS 0x3

typedef struct tserv_request {
	tserv_request_type_t request_type;
	tserv_request_id_t request_id;
} tserv_request_t;

typedef struct tserv_request_set_runlevel {
	tserv_request_type_t request_type;
	tserv_request_id_t request_id;
	tserv_string_t runlevel;
} tserv_request_set_runlevel_t;

typedef struct tserv_request_get_runlevel {
	tserv_request_type_t request_type;
	tserv_request_id_t request_id;
} tserv_request_get_runlevel_t;

typedef struct tserv_request_get_runlevel_info {
	tserv_request_type_t request_type;
	tserv_request_id_t request_id;
	tserv_string_t runlevel;
} tserv_request_get_runlevel_info_t;

typedef struct tserv_request_start_service {
	tserv_request_type_t request_type;
	tserv_request_id_t request_id;
	tserv_string_t service;
} tserv_request_start_service_t;

typedef struct tserv_request_end_service {
	tserv_request_type_t request_type;
	tserv_request_id_t request_id;
	tserv_string_t service;
} tserv_request_end_service_t;

typedef struct tserv_request_stop_service {
	tserv_request_type_t request_type;
	tserv_request_id_t request_id;
	tserv_string_t service;
} tserv_request_stop_service_t;

typedef struct tserv_request_continue_service {
	tserv_request_type_t request_type;
	tserv_request_id_t request_id;
	tserv_string_t service;
} tserv_request_continue_service_t;

typedef struct tserv_request_get_service_status {
	tserv_request_type_t request_type;
	tserv_request_id_t request_id;
	tserv_string_t service;
} tserv_request_get_service_status_t;

typedef struct tserv_request_reload_conf {
	tserv_request_type_t request_type;
	tserv_request_id_t request_id;
} tserv_request_reload_conf_t;

// responses

typedef struct tserv_response {
	tserv_response_type_t response_type;
	tserv_request_id_t request_id;
} tserv_response_t;

typedef struct tserv_response_success {
	tserv_response_type_t response_type;
	tserv_request_id_t request_id;
} tserv_response_success_t;

typedef struct tserv_response_error {
	tserv_response_type_t response_type;
	tserv_request_id_t request_id;
	tserv_error_t error;
} tserv_response_error_t;

typedef struct tserv_response_service_status {
	tserv_response_type_t response_type;
	tserv_request_id_t request_id;
	tserv_service_status_t status;
} tserv_response_service_status_t;

#endif
