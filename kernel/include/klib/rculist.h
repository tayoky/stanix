#ifndef KERNEL_RCULIST_H
#define KERNEL_RCULIST_H

#include <kernel/list.h>
#include <kernel/rcu.h>

typedef struct rculist_node {
	rcu_ptr_t prev;
	rcu_ptr_t next;
} rculist_node_t;

/**
 * @brief a rculist can only be traversed from first to last
 */
typedef struct rculist {
	rcu_ptr_t first;
	rcu_ptr_t last;
	spinlock_t lock;
} rculist_t;

static inline void rculist_acquire_read(rculist_t *rculist) {
	(void)rculist;
	rcu_acquire_read();
}

static inline void rculist_release_read(rculist_t *rculist) {
	(void)rculist;
	rcu_release_read();
}

static inline void rculist_acquire_write(rculist_t *rculist) {
	spinlock_acquire(&rculist->lock);
}

static inline void rculist_release_write(rculist_t *rculist) {
	spinlock_release(&rculist->lock);
}

static inline rculist_node_t *rculist_get_first(rculist_t *rculist) {
	return rcu_ptr_fetch(&rculist->first);
}

static inline rculist_is_empty(rculist_t *rculist) {
	return rculist_get_first(rculist) == NULL;
}

static inline rculist_node_t *rculist_get_next(rculist_node_t *node) {
	return rcu_ptr_fetch(&node->next);
}

void rculist_init(rculist_t *rculist);
void rculist_destroy(rculist_t *rculist);
void rculist_raw_prepend(rculist_t *rculist, rculist_node_t *node);
void rculist_raw_append(rculist_t *rculist, rculist_node_t *node);
void rculist_raw_remove(rculist_t *rculist, rculist_node_t *node);
void rculist_raw_add_after(rculist_t *rculist, rculist_node_t *ref, rculist_node_t *node);
void rculist_raw_add_before(rculist_t *rculist, rculist_node_t *ref, rculist_node_t *node);
void rculist_prepend(rculist_t *rculist, rculist_node_t *node);
void rculist_append(rculist_t *rculist, rculist_node_t *node);
void rculist_remove(rculist_t *rculist, rculist_node_t *node);
void rculist_add_after(rculist_t *rculist, rculist_node_t *ref, rculist_node_t *node);
void rculist_add_before(rculist_t *rculist, rculist_node_t *ref, rculist_node_t *node);

#define rculist_foreach(node, rculist) \
	for (rculist_node_t *node = rculist_get_first(rculist); node; node = rculist_get_next(node))

#endif
