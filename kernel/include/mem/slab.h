#ifndef _KERNEL_SLAB_H
#define _KERNEL_SLAB_H

#include <kernel/spinlock.h>
#include <kernel/list.h>
#include <stddef.h>

struct slab_cache;

typedef struct slab_free_node {
    struct slab_free_node *next;
} slab_free_node_t;

typedef struct slab {
    list_node_t node;
    struct slab_cache *cache;
    slab_free_node_t *free;
    size_t allocated_count;
    int state;
} slab_t;

#define SLAB_FREE    0
#define SLAB_PARTIAL 1
#define SLAB_FULL    2

typedef struct slab_cache {
    int (*constructor)(struct slab_cache*,void *);
    int (*destructor)(struct slab_cache*,void *);
    void *(*evict)(struct slab_cache*);
    const char *name;
    list_t free;
    list_t partial;
    list_t full;
    spinlock_t lock;
    size_t size;
} slab_cache_t;

int slab_init(slab_cache_t *slab_cache, size_t size, const char *name);
void slab_destroy(slab_cache_t *slab_cache);
void *slab_alloc(slab_cache_t *slab_cache);
void slab_free(void *ptr);
void *slab_evict(slab_cache_t *slab_cache);

#endif
