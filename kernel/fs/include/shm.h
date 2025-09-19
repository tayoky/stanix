#ifndef _KERNEL_SHM_H
#define _KERNEL_SHM_H

#include <limits.h>
#include <sys/types.h>
#include <kernel/list.h>

typedef struct shm_file {
    char name[PATH_MAX];
    size_t size;
    list *blocks;
    uid_t uid;
    gid_t gid;
    mode_t mode;
    size_t ref_count;
} shm_file;


void init_shm(void);

#endif