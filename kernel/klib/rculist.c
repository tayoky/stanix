#include <kernel/rculist.h>
#include <kernel/string.h>

void rculist_init(rculist_t *rculist) {
	memset(rculist, 0, sizeof(rculist_t));
}

void rculist_destroy(rculist_t *rculist) {
	(void)rculist;
}

void rculist_prepend(rculist_t *rculist, rculist_node_t *node) {
	spinlock_acquire(&rculist->lock);

	// link
	node->prev = NULL;
	node->next = rculist->first;
	if (rculist->first) {
		rculist->first->prev = node;
	} else {
		rculist->last = node;
	}
	rcu_ptr_store(&rculist->first, node);

	spinlock_release(&rculist->lock);
}

void rculist_append(rculist_t *rculist, rculist_node_t *node) {
	spinlock_acquire(&rculist->lock);

	// link
	node->prev = rculist->last;
	node->next = NULL;
	rculist->last = node;
	if (node->prev) {
		rcu_ptr_store(&node->prev->next, node);
	} else {
		rcu_ptr_store(&rculist->first, node);
	}

	spinlock_release(&rculist->lock);
}

void rculist_remove(rculist_t *rculist, rculist_node_t *node) {
	if (!node) return;
	spinlock_acquire(&rculist->lock);
	if (node->next) {
		node->next->prev = node->prev;
	} else {
		rculist->last = node->prev;
	}
	if (node->prev) {
		rcu_ptr_store(&node->prev->next, node->next);
	} else {
		rcu_ptr_store(&rculist->first, node->next);
	}

	spinlock_release(&rculist->lock);
}

// TODO : implement theses
void rculist_add_after(rculist_t *rculist, rculist_node_t *ref, rculist_node_t *node);
void rculist_add_before(rculist_t *rculist, rculist_node_t *ref, rculist_node_t *node) {
