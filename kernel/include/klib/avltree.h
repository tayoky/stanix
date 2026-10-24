#ifndef KERNEL_AVLTREE_H
#define KERNEL_AVLTREE_H

#include <stddef.h>

typedef struct avltree_ops avltree_ops_t;
typedef struct avltree_node avltree_node_t;
typedef struct avltree avltree_t;
typedef int (*avltree_compare_func_t)(avltree_t *avltree, void *a, void *b);

struct avltree_ops {
	int (*compare_nodes)(avltree_t *avltree, avltree_node_t *a, avltree_node_t *b);
	int (*compare_key)(avltree_t *avltree, void *key, avltree_node_t *node);
	int (*search_leftest)(avltree_t *avltree, void *key, avltree_node_t *node);
	void (*recalculate)(avltree_t *avltree, avltree_node_t *node);
};

struct avltree_node {
	avltree_node_t *parent;
	avltree_node_t *left;
	avltree_node_t *right;
	size_t height;
};

struct avltree {
	avltree_node_t *root;
	avltree_ops_t *ops;
};

static inline void avltree_init(avltree_t *avltree, avltree_ops_t *ops) {
	avltree->root = NULL;
	avltree->ops  = ops;
}

static inline void avltree_destroy(avltree_t *avltree) {
	(void)avltree;
}

void avltree_insert(avltree_t *avltree, avltree_node_t *node);
int avltree_remove(avltree_t *avltree, avltree_node_t *node);
avltree_node_t *avltree_lookup(avltree_t *avltree, void *key);
avltree_node_t *avltree_first_superior_or_equal(avltree_t *avltree, void *key);
avltree_node_t *avltree_first(avltree_t *avltree);
avltree_node_t *avltree_next(avltree_t *avltree, avltree_node_t *node);
avltree_node_t *avltree_first_key(avltree_t *avltree, void *key);
avltree_node_t *avltree_next_key(avltree_t *avltree, avltree_node_t *node, void *key);

#define avltree_foreach(node, avltree) \
	for (avltree_node_t *node = avltree_first(avltree); \
		node; \
		node = avltree_next(avltree, node))

#define avltree_foreach_key(node, avltree, key) \
	for (avltree_node_t *node = avltree_first_key(avltree, key); \
		node; \
		node = avltree_next_key(avltree, node, key))

#endif
