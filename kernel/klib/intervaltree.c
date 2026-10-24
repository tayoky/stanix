#include <kernel/intervaltree.h>

static int interval_tree_compare_nodes(avltree_t *avltree, avltree_node_t *a, avltree_node_t *b) {
	(void)avltree;
	interval_tree_node_t *node_a = container_of(a, interval_tree_node_t, node);
	interval_tree_node_t *node_b = container_of(b, interval_tree_node_t, node);
	if (node_a->span.start < node_b->span.start) {
		return -1;
	} else if (node_a->span.start > node_b->span.start) {
		return 1;
	} else if (node_a->span.end < node_b->span.end) {
		return -1;
	} else if (node_a->span.end > node_b->span.end) {
		return 1;
	} else {
		return 0;
	}
}

static int interval_tree_compare_key(avltree_t *avltree, void *key, avltree_node_t *avltree_node) {
	(void)avltree;
	interval_tree_span_t *span = key;
	interval_tree_node_t *node = container_of(avltree_node, interval_tree_node_t, node);
	if (node->node.left) {
		interval_tree_node_t *left = container_of(node->node.left, interval_tree_node_t, node);
		if (left->max >= span->start) {
			return -1;
		}
	}
	if (span->start < node->span.end && span->end > node->span.end) {
		return 0;
	}
	return 1;
}

static void interval_tree_recalculate(avltree_t *avltree, avltree_node_t *avltree_node) {
	(void)avltree;
	interval_tree_node_t *node = container_of(avltree_node, interval_tree_node_t, node);
	node->max = node->span.end;
	if (node->node.left) {
		interval_tree_node_t *left = container_of(node->node.left, interval_tree_node_t, node);
		if (left->max > node->max) node->max = left->max;
	}
	if (node->node.right) {
		interval_tree_node_t *right = container_of(node->node.right, interval_tree_node_t, node);
		if (right->max > node->max) node->max = right->max;
	}
}

static avltree_ops_t interval_tree_ops = {
	.compare_nodes  = interval_tree_compare_nodes,
	.compare_key    = interval_tree_compare_key,
	.recalculate    = interval_tree_recalculate,
};

void interval_tree_init(interval_tree_t *tree) {
	avltree_init(&tree->avltree, &interval_tree_ops);
}

void interval_tree_destroy(interval_tree_t *tree) {
	avltree_destroy(&tree->avltree);
}
