#include <kernel/list.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/mutex.h>

list *new_list(){
	list *l = kmalloc(sizeof(list));
	l->frist_node = NULL;
	l->last_node = NULL;
	l->node_count = 0;
	init_mutex(&l->mutex);
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

	acquire_mutex(&l->mutex);

	//link
	new_node->prev = l->last_node;
	if(l->last_node){
		l->last_node->next = new_node;
	}else {
		l->frist_node = new_node;
	}
	l->last_node = new_node;

	l->node_count++;
	release_mutex(&l->mutex);
}

void list_add_after(list *l,list_node *node,void *value){
	if(node) {
		list_node *new_node = kmalloc(sizeof(list_node));
		new_node->value = value;
		acquire_mutex(&l->mutex);
		
		//link
		new_node->prev = node;
		new_node->next = node->next;
		node->next = new_node;
		if(new_node->next->prev){
			new_node->next->prev = new_node;
		}
	} else {
		list_node *new_node = kmalloc(sizeof(list_node));
		new_node->value = value;
		new_node->prev = NULL;
		acquire_mutex(&l->mutex);

		//link
		if(l->frist_node){
			l->frist_node->prev = new_node;
		}
		new_node->next = l->frist_node;
		l->frist_node = new_node;
	}

	l->node_count++;
	release_mutex(&l->mutex);
}

void list_remove(list *l,void *value){
	acquire_mutex(&l->mutex);
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
			release_mutex(&l->mutex);
			return;
		}
	}
	release_mutex(&l->mutex);
}