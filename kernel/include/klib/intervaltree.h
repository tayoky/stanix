#ifndef KERNEL_INTERVALTREE_H
#define KERNEL_INTERVALTREE_H

#include <kernel/avltree.h>
#include <kernel/macro.h>

typedef struct interval_tree {
	avltree_t avltree;
} interval_tree_t;

typedef struct interval_tree_span {
	size_t start;
	size_t end;
} interval_tree_span_t;

typedef struct interval_tree_node {
	avltree_node_t node;
	interval_tree_span_t span;
	size_t max;
} interval_tree_node_t;

void interval_tree_init(interval_tree_t *tree);
void interval_tree_destroy(interval_tree_t *tree);

static inline void interval_tree_insert(interval_tree_t *tree, interval_tree_node_t *node) {
	avltree_insert(&tree->avltree, &node->node);
}

static inline int interval_tree_remove(interval_tree_t *tree, interval_tree_node_t *node) {
	return avltree_remove(&tree->avltree, &node->node);
}

static inline interval_tree_node_t *interval_tree_first(interval_tree_t *tree) {
	avltree_node_t *avltree_node = avltree_first(&tree->avltree);
	if (!avltree_node) return NULL;
	return container_of(avltree_node, interval_tree_node_t, node);
}

static inline interval_tree_node_t *interval_tree_next(interval_tree_t *tree, interval_tree_node_t *node) {
	if (!node) return NULL;
	avltree_node_t *avltree_node = avltree_next(&tree->avltree, &node->node);
	if (!avltree_node) return NULL;
	return container_of(avltree_node, interval_tree_node_t, node);
}

static inline interval_tree_node_t *interval_tree_first_span(interval_tree_t *tree, interval_tree_span_t *span) {
	avltree_node_t *avltree_node = avltree_first_superior_or_equal(&tree->avltree, span);
	if (!avltree_node) return NULL;
	interval_tree_node_t *node = container_of(avltree_node, interval_tree_node_t, node);
	if (node->start >= span->end) return NULL;
	return node;
}

interval_tree_node_t *interval_tree_next_span(interval_tree_t *tree, interval_tree_node_t *node, interval_tree_span_t *span) {
	if (!node) return NULL;
	avltree_node_t *avltree_node = avltree_next(&tree->avltree, &node->node);
	if (!avltree_node) return NULL;
	interval_tree_node_t *node = container_of(avltree_node, interval_tree_node_t, node);
	if (node->start >= span->end) return NULL;
	return node;
}

#define interval_tree_foreach(node, tree) \
	for (interval_tree_t *node = interval_tree_first(tree); \
		node; \
		node = interval_tree_next(tree, node))

#define interval_tree_foreach_span(node, tree, span) \
	for (interval_tree_t *node = interval_tree_first_span(tree, span); \
		node; \
		node = interval_tree_next_span(tree, node, span))

#define interval_foreach_range(node, tree, start, end) \
	loop_var_value(interval_tree_span_t, _span, {(start), (end)}) \
	interval_tree_foreach_span(node, tree, &_span)

#endif
