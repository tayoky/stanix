#ifndef LIST_H
#define LIST_H

#include <kernel/spinlock.h>
#include <sys/type.h>

struct list_struct;

typedef struct list_node_struct{
	struct list_struct *list;
	struct list_node_struct *next;
	struct list_node_struct *prev;
	void *value;
} list_node;

typedef struct list {
	list_node *frist_node;
	list_node *last_node;
	size_t node_count;
	spinlock lock;
} list_t;

list_t *new_list();
void free_list(list_t *l);
void list_append(list_t *l,void *value);
void list_remove(list_t *l,void *value);
void list_add_after(list_t *l,list_node *node,void *value);
void list_remove_node(list_t *l,list_node *node);

#define foreach(var,l) for(list_node * var = l->frist_node ; var ; var = var ->next)

#endif