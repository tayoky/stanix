#ifndef _KERNEL_TRM_H
#define _KERNEL_TRM_H

#include <libtrm/trm.h>
#include <kernel/device.h>
#include <kernel/list.h>
#include <libutils/hashmap.h>
#include <stdint.h>

struct vfs_fd;
struct trm_gpu;
struct memseg_struct;

typedef struct trm_ops {
    int (*test_mode)(struct trm_gpu *gpu, trm_mode_t *mode);
    int (*fix_mode)(struct trm_gpu *gpu, trm_mode_t *mode);
    int (*commit_mode)(struct trm_gpu *gpu, trm_mode_t *mode);
    void (*cleanup)(struct trm_gpu *gpu);
    int (*mmap)(struct trm_gpu *gpu, uintptr_t vaddr, struct memseg_struct *seg);
} trm_ops_t;

typedef struct trm_alloc_block {
	uintptr_t base;
	size_t size;
	int free;
} trm_alloc_block_t;

typedef struct trm_framebuffer {
	struct vfs_fd *owner;
	struct vfs_fd *fd;
	struct trm_gpu *gpu;
	uintptr_t base;
	trm_fb_t fb;
} trm_framebuffer_t;

typedef struct trm_gpu {
    device_t device;
    trm_card_t card;
    trm_mode_t mode;
    trm_ops_t *ops;
    list_t *alloc_blocks;
    libutils_hashmap_t fbs;
    struct vfs_fd *master;
    uint32_t fb_count;
} trm_gpu_t;

trm_framebuffer_t *trm_get_fb(trm_gpu_t *gpu, uint32_t id);
int register_trm_gpu(trm_gpu_t *gpu);

#endif
