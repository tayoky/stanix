#include <kernel/avltree.h>
#include <kernel/string.h>

static int avltree_compare_nodes(avltree_t *avltree, avltree_node_t *a, avltree_node_t *b) {
	int ret = avltree->ops->compare_nodes(avltree, a, b);
	if (ret < 0) {
		return -1;
	} else if (ret > 0) {
		return 1;
	} else {
		return 0;
	}
}

static int avltree_compare_key(avltree_t *avltree, void *key, avltree_node_t *node) {
	int ret = avltree->ops->compare_key(avltree, key, node);
	if (ret < 0) {
		return -1;
	} else if (ret > 0) {
		return 1;
	} else {
		return 0;
	}
}

static int avltree_height(avltree_node_t *node) {
	return node ? node->height : 0;
}

static void avltree_compute_height(avltree_t *avltree, avltree_node_t *node) {
	int left_height  = avltree_height(node->left);
	int right_height = avltree_height(node->right);
	node->height = (left_height > right_height ? left_height : right_height) + 1;
	if (avltree->ops->recalculate) {
		avltree->ops->recalculate(avltree, node);
	}
}

static int avltree_balance_factor(avltree_node_t *node) {
	return avltree_height(node->right) - avltree_height(node->left);
}

static avltree_node_t *avltree_rotate_left(avltree_t *avltree, avltree_node_t *node) {
	avltree_node_t *right = node->right;
	node->right = right->left;
	right->left = node;

	avltree_compute_height(avltree, node);
	avltree_compute_height(avltree, right);
	return right;
}

static avltree_node_t *avltree_rotate_right(avltree_t *avltree, avltree_node_t *node) {
	avltree_node_t *left = node->left;
	node->left = left->right;
	left->right = node;

	avltree_compute_height(avltree, node);
	avltree_compute_height(avltree, left);
	return left;
}

static avltree_node_t *avltree_leftest_subnode(avltree_node_t *node) {
	while (node->left) {
		node = node->left;
	}
	return node;
}

static void avltree_rebalance(avltree_t *avltree, avltree_node_t **current, avltree_node_t **child) {
	switch (avltree_balance_factor(*current)) {
	case -2:
		// left heavy
		if (avltree_balance_factor(*child) > 0) {
			// left right rotation
			*child = avltree_rotate_left(avltree, *child);
		}
		*current = avltree_rotate_right(avltree, *current);
		break;
	case -1:
	case 0:
	case 1:
		// balanced enough, no rotation needed
		avltree_compute_height(avltree, *current);
		break;
	case 2:
		// right heavy
		if (avltree_balance_factor(*child) < 0) {
			// right left rotation
			*child = avltree_rotate_right(avltree, *child);
		}
		*current = avltree_rotate_left(avltree, *current);
		break;
	default:
		__builtin_unreachable();
	}
}

static int avltree_insert_helper(avltree_t *avltree, avltree_node_t **parent, avltree_node_t *node) {
	if (!*parent) {
		*parent = node;
		return 1;
	}

	avltree_node_t **child;
	switch (avltree_compare_nodes(avltree, node, *parent)) {
	case -1:
		child = &(*parent)->left;
		break;
	case 1:
		child = &(*parent)->right;
		break;
	case 0:
		// cannot have twice the same value
		return 0;
	default:
		__builtin_unreachable();
	}
	
	if (!avltree_insert_helper(avltree, child, node)) return 0;

	avltree_rebalance(avltree, parent, child);
	return 1;
}

int avltree_raw_insert(avltree_t *avltree, avltree_node_t *node) {
	rwsem_assert_write_acquired(&avltree->rwsem);
	node->height = 1;

	return avltree_insert_helper(avltree, &avltree->root, node);
}

static int avltree_remove_helper(avltree_t *avltree, avltree_node_t **current, avltree_node_t *node) {
	if (!*current) return 0;

	avltree_node_t **child;
	switch (avltree_compare_nodes(avltree, node, *current)) {
	case -1:
		child = &(*current)->left;
		break;
	case 1:
		child = &(*current)->right;
		break;
	case 0:;
		// this is the node to delete
		avltree_node_t *node = *current;
		if (!node->right) {
			// no children or only left children
			*current = node->left;
		} else if (!node->left) {
			// only right children
			*current = node->right;
		} else {
			// both left and right children
			avltree_node_t *leftest = avltree_leftest_subnode(node->right);

			// remove the leftest subnode of right
			if (!avltree_remove_helper(avltree, &node->right, leftest)) {
				// normally we must find the leftest node
				__builtin_unreachable();
			}

			// and replace it over us
			leftest->left  = node->left;
			leftest->right = node->right;
			*current = leftest;
			avltree_rebalance(avltree, current, &leftest->right);
		}
		return 1;
	default:
		__builtin_unreachable();
	}

	if (!avltree_remove_helper(avltree, child, node)) return 0;

	avltree_rebalance(avltree, current, child);
	return 1;
}
 
int avltree_raw_remove(avltree_t *avltree, avltree_node_t *node) {
	rwsem_assert_write_acquired(&avltree->rwsem);
	return avltree_remove_helper(avltree, &avltree->root, node);
}

avltree_node_t *avltree_raw_lookup(avltree_t *avltree, void *key) {
	avltree_node_t *node = avltree->root;
	while (node) {
		switch (avltree_compare_key(avltree, key, node)) {
		case -1:
			node = node->left;
			break;
		case 1:
			node = node->right;
			break;
		case 0:
			return node;
		default:
			__builtin_unreachable();
		}
	}
	return NULL;
}

int avltree_insert(avltree_t *avltree, avltree_node_t *node) {
	avltree_acquire_write(avltree);
	int ret = avltree_raw_insert(avltree, node);
	avltree_release_write(avltree);
	return ret;
}

int avltree_remove(avltree_t *avltree, avltree_node_t *node) {
	avltree_acquire_write(avltree);
	int ret = avltree_raw_remove(avltree, node);
	avltree_release_write(avltree);
	return ret;
}

avltree_node_t *avltree_lookup(avltree_t *avltree, void *key) {
	avltree_acquire_read(avltree);
	avltree_node_t *node = avltree_raw_lookup(avltree, key);
	avltree_release_read(avltree);
	return node;
}
