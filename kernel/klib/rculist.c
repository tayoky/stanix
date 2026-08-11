#include <kernel/rculist.h>
#include <kernel/string.h>

void rculist_init(rculist_t *rculist) {
	memset(rculist, 0, sizeof(rculist_t));
}

void rculist_destroy(rculist_t *rculist) {
	(void)rculist;
}

// internal stuff

static inline rculist_node_t *rculist_fetch_ptr(rcu_ptr_t *rcu_ptr) {
	return rcu_ptr_fetch(rcu_ptr);
}

static inline void rculist_store_ptr(rcu_ptr_t *rcu_ptr, rculist_node_t *value) {
	rcu_ptr_store(rcu_ptr, value);
}

void rculist_prepend(rculist_t *rculist, rculist_node_t *node) {
	spinlock_acquire(&rculist->lock);

	// link
	node->prev = NULL;
	node->next = rculist->first;
	if (rculist->first) {
		rculist_node_t *first = rculist_fetch_ptr(&rculist->first);
		rculist_store_ptr(&first->prev, node);
	} else {
		rculist->last = node;
	}
	rculist_store_ptr(&rculist->first, node);

	spinlock_release(&rculist->lock);
}

void rculist_append(rculist_t *rculist, rculist_node_t *node) {
	spinlock_acquire(&rculist->lock);

	// link
	node->prev = rculist->last;
	node->next = NULL;
	rculist->last = node;
	if (node->prev) {
		rculist_node_t *prev = rculist_fetch_ptr(&node->prev);
		rculist_store_ptr(&prev->next, node);
	} else {
		rculist_store_ptr(&rculist->first, node);
	}

	spinlock_release(&rculist->lock);
}

void rculist_remove(rculist_t *rculist, rculist_node_t *node) {
	if (!node) return;
	spinlock_acquire(&rculist->lock);
	if (node->next) {
		rculist_node_t *next = rculist_fetch_ptr(&node->next);
		rculist_store_ptr(&next->prev, node->prev);
	} else {
		rculist_store_ptr(&rculist->last, node->prev);
	}
	if (node->prev) {
		rculist_node_t *prev = rculist_fetch_ptr(&node->prev);
		rculist_store_ptr(&prev->next, node->next);
	} else {
		rculist_store_ptr(&rculist->first, node->next);
	}

	spinlock_release(&rculist->lock);
}

// TODO : implement theses
void rculist_add_after(rculist_t *rculist, rculist_node_t *ref, rculist_node_t *node);
void rculist_add_before(rculist_t *rculist, rculist_node_t *ref, rculist_node_t *node);	
