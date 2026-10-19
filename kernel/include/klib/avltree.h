#ifndef KERNEL_AVLTREE_H
#define KERNEL_AVLTREE_H

#include <kernel/rwsem.h>
#include <stddef.h>

typedef struct avltree_node avltree_node_t;
typedef struct avltree avltree_t;
typedef int (*avltree_compare_func_t)(avltree_t *avltree, void *a, void *b);

struct avltree_node {
	avltree_node_t *left;
	avltree_node_t *right;
	void *key;
	int height;
};

struct avltree {
	rwsem_t rwsem;
	avltree_node_t *root;
	avltree_compare_func_t compare;
};

static inline void avltree_init(avltree_t *avltree, avltree_compare_func_t compare) {
	avltree->root = NULL;
	avltree->compare = compare;
}

static inline void avltree_destroy(avltree_t *avltree) {
	(void)avltree;
}

static void avltree_acquire_read(avltree_t *avltree) {
	rwsem_acquire_read(&avltree->rwsem);
}

static void avltree_release_read(avltree_t *avltree) {
	rwsem_release_read(&avltree->rwsem);
}

static void avltree_acquire_write(avltree_t *avltree) {
	rwsem_acquire_write(&avltree->rwsem);
}

static void avltree_release_write(avltree_t *avltree) {
	rwsem_release_write(&avltree->rwsem);
}

int avltree_raw_insert(avltree_t *avltree, avltree_node_t *node, void *key);
int avltree_raw_remove(avltree_t *avltree, void *key);
avltree_node_t *avltree_raw_lookup(avltree_t *avltree, void *key);
int avltree_insert(avltree_t *avltree, avltree_node_t *node, void *key);
int avltree_remove(avltree_t *avltree, void *key);
avltree_node_t *avltree_lookup(avltree_t *avltree, void *key);

#endif
