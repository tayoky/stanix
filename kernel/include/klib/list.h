#ifndef _KERNEL_LIST_H
#define _KERNEL_LIST_H

#include <kernel/spinlock.h>
#include <kernel/macro.h>
#include <sys/type.h>

typedef struct list_node {
	struct list_node *next;
	struct list_node *prev;
} list_node_t;

typedef struct list {
	list_node_t *first_node;
	list_node_t *last_node;
	size_t node_count;
	spinlock_t lock;
} list_t;

void init_list(list_t *list);
void destroy_list(list_t *list);
void list_append(list_t *list, list_node_t *node);
void list_remove(list_t *list, list_node_t *node);
void list_add_after(list_t *list, list_node_t *ref, list_node_t *node);

#define foreach(var,l) for(list_node_t * var = (l)->first_node ; var ; var = var ->next)

#endif