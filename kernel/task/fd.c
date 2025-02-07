#include "fd.h"
#include "scheduler.h"

int is_vaild_fd(int fd){
	return get_current_proc()->fds[fd].present & 1;
}