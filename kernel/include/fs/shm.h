#ifndef _KERNEL_SHM_H
#define _KERNEL_SHM_H

#include <limits.h>
#include <sys/types.h>
#include <kernel/list.h>

typedef struct shm_file {
    list_node_t node;
    char name[PATH_MAX];
    size_t size;
    list_t blocks;
    uid_t uid;
    gid_t gid;
    mode_t mode;
    size_t ref_count;
} shm_file_t;

typedef struct shm_block {
    list_node_t node;
    uintptr_t block;
} shm_block_t;

void init_shm(void);

#endif