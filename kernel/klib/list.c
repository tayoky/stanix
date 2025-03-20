#include "list.h"
#include "kheap.h"
#include "string.h"

list *new_list(){
	list *l = kmalloc(sizeof(list));
	l->frist_node = NULL;
	l->last_node = NULL;
	l->node_count = 0;
	return l;
}

void free_list(list *l){
	list_node *prev = NULL;
	foreach(node,l){
		if(prev){
			kfree(prev);
		}
		prev = node;
	}
	if(prev){
		kfree(prev);
	}

	kfree(l);
}

void list_append(list *l,void *value){
	list_node *new_node = kmalloc(sizeof(list_node));
	memset(new_node,0,sizeof(list_node));
	new_node->value = value;
	if(l->last_node){
		new_node->prev = l->last_node;
		l->last_node->next = new_node;
		l->last_node = new_node;
	}else {
		l->last_node = new_node;
		l->frist_node = new_node;
	}

	l->node_count++;
}

void list_add_after(list *l,list_node *node,void *value){
	if(node == l->last_node){
		return list_append(l,value);
	} else if (node){
		list_node *new_node = kmalloc(sizeof(list_node));
		new_node->prev = node;
		new_node->next = node->next;
		new_node->value = value;

		node->next = new_node;
		new_node->next->prev = new_node;
	} else {
		list_node *new_node = kmalloc(sizeof(list_node));
		new_node->prev = NULL;
		new_node->next = l->frist_node;
		new_node->value = value;
		l->frist_node = new_node;
	}

	l->node_count++;
}

void list_remove(list *l,void *value){
	foreach(node,l){
		if(node->value == value){
			if(node->prev){
				node->prev->next = node->next;
			} else {
				l->frist_node = node->next;
			}
			if(node->next){
				node->next->prev = node->prev;
			} else {
				l->last_node = node->prev;
			}
			l->node_count--;
			return;
		}
	}

	return;
}