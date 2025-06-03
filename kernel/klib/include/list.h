#ifndef LIST_H
#define LIST_H

#include <kernel/mutex.h>
#include <sys/type.h>

struct list_struct;

typedef struct list_node_struct{
	struct list_struct *list;
	struct list_node_struct *next;
	struct list_node_struct *prev;
	void *value;
} list_node;

typedef struct list_struct{
	list_node *frist_node;
	list_node *last_node;
	size_t node_count;
	mutex_t mutex;
} list;

list *new_list();
void free_list(list *l);
void list_append(list *l,void *value);
void list_remove(list *l,void *value);
void list_add_after(list *l,list_node *node,void *value);

#define foreach(var,l) for(list_node * var = l->frist_node ; var ; var = var ->next)

#endif