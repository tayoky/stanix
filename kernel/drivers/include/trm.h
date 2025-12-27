#ifndef _KERNEL_TRM_H
#define _KERNEL_TRM_H

#include <libtrm/trm.h>
#include <kernel/device.h>

struct vfs_fd;
struct trm_gpu;

typedef struct trm_ops {
    int (*test_mode)(struct trm_gpu *gpu, trm_mode_t *mode);
    int (*fix_mode)(struct trm_gpu *gpu, trm_mode_t *mode);
    int (*commit_mode)(struct trm_gpu *gpu, trm_mode_t *mode);
} trm_ops_t;

typedef struct trm_gpu {
    device_t device;
    trm_card_t card;
    trm_mode_t mode;
    trm_ops_t *ops;
    struct vfs_fd *master;
} trm_gpu_t;

int register_trm_gpu(trm_gpu_t *gpu);

#endif