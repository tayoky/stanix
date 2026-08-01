#include <kernel/list.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>

void list_init(list_t *list) {
	memset(list, 0, sizeof(list_t));
}

void list_destroy(list_t *list) {
	(void)list;
}

void list_prepend(list_t *list, list_node_t *node) {
	spinlock_acquire(&list->lock);

	// link
	node->prev = NULL;
	node->next = list->first_node;
	if (list->first_node) {
		list->first_node->prev = node;
	} else {
		list->last_node = node;
	}
	list->first_node = node;

	list->node_count++;
	spinlock_release(&list->lock);
}

void list_append(list_t *list, list_node_t *node) {
	spinlock_acquire(&list->lock);

	// link
	node->prev = list->last_node;
	node->next = NULL;
	if (list->last_node) {
		list->last_node->next = node;
	} else {
		list->first_node = node;
	}
	list->last_node = node;

	list->node_count++;
	spinlock_release(&list->lock);
}

void list_add_after(list_t *list, list_node_t *ref, list_node_t *node) {
	if (!ref) {
		list_prepend(list, node);
		return;
	}

	// link
	spinlock_acquire(&list->lock);
	node->prev = ref;
	node->next = ref->next;
	if (ref->next) {
		ref->next->prev = node;
	} else {
		list->last_node = node;
	}
	ref->next = node;
	list->node_count++;
	spinlock_release(&list->lock);
}

void list_add_before(list_t *list, list_node_t *ref, list_node_t *node) {
	if (ref) {
		list_add_after(list, ref->prev, node);
	} else {
		list_append(list, node);
	}
}

void list_remove(list_t *list, list_node_t *node) {
	if (!node) return;
	spinlock_acquire(&list->lock);
	if (node->prev) {
		node->prev->next = node->next;
	} else {
		list->first_node = node->next;
	}
	if (node->next) {
		node->next->prev = node->prev;
	} else {
		list->last_node = node->prev;
	}
	list->node_count--;

	spinlock_release(&list->lock);
}
