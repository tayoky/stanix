#include <kernel/avltree.h>
#include <kernel/string.h>

static int avltree_compare(avltree_t *avltree, void *a, void *b) {
	int ret = avltree->compare(avltree, a, b);
	if (ret < 0) {
		return -1;
	} else if (ret > 0) {
		return 1;
	} else {
		return 0;
	}
}

static int avltree_compare_nodes(avltree_t *avltree, avltree_node_t *a, avltree_node_t *b) {
	return avltree_compare(avltree, a->key, b->key);
}

static int avltree_height(avltree_node_t *node) {
	return node ? node->height : 0;
}

static void avltree_compute_height(avltree_node_t *node) {
	int left_height  = avltree_height(node->left);
	int right_height = avltree_height(node->right);
	node->height = (left_height > right_height ? left_height : right_height) + 1;
}

static int avltree_balance_factor(avltree_node_t *node) {
	return avltree_height(node->right) - avltree_height(node->left);
}

static avltree_node_t *avltree_rotate_left(avltree_node_t *node) {
	avltree_node_t *right = node->right;
	node->right = right->left;
	right->left = node;

	avltree_compute_height(node);
	avltree_compute_height(right);
	return right;
}

static avltree_node_t *avltree_rotate_right(avltree_node_t *node) {
	avltree_node_t *left = node->left;
	node->left = left->right;
	left->right = node;

	avltree_compute_height(node);
	avltree_compute_height(left);
	return left;
}

static avltree_node_t *avltree_leftest_subnode(avltree_node_t *node) {
	while (node->left) {
		node = node->left;
	}
	return node;
}

static void avltree_rebalance(avltree_node_t **current, avltree_node_t **child) {
	switch (avltree_balance_factor(*current)) {
	case -2:
		// left heavy
		if (avltree_balance_factor(*child) > 0) {
			// left right rotation
			*child = avltree_rotate_left(*child);
		}
		*current = avltree_rotate_right(*current);
		break;
	case -1:
	case 0:
	case 1:
		// balanced enough, no rotation needed
		avltree_compute_height(*current);
		break;
	case 2:
		// right heavy
		if (avltree_balance_factor(*child) < 0) {
			// right left rotation
			*child = avltree_rotate_right(*child);
		}
		*current = avltree_rotate_left(*current);
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

	avltree_rebalance(parent, child);
	return 1;
}

int avltree_raw_insert(avltree_t *avltree, avltree_node_t *node, void *key) {
	rwsem_assert_write_acquired(&avltree->rwsem);
	node->key = key;
	node->height = 1;

	return avltree_insert_helper(avltree, &avltree->root, node);
}

static int avltree_remove_helper(avltree_t *avltree, avltree_node_t **current, void *key) {
	if (!*current) return 0;

	avltree_node_t **child;
	switch (avltree_compare(avltree, key, (*current)->key)) {
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
			if (!avltree_remove_helper(avltree, &node->right, leftest->key)) {
				// normally we must find the leftest node
				__builtin_unreachable();
			}

			// and replace it over us
			leftest->left  = node->left;
			leftest->right = node->right;
			*current = leftest;
			avltree_rebalance(current, &leftest->right);
		}
		return 1;
	default:
		__builtin_unreachable();
	}

	if (!avltree_remove_helper(avltree, child, key)) return 0;

	avltree_rebalance(current, child);
	return 1;
}
 
int avltree_raw_remove(avltree_t *avltree, void *key) {
	rwsem_assert_write_acquired(&avltree->rwsem);
	return avltree_remove_helper(avltree, &avltree->root, key);
}

avltree_node_t *avltree_raw_lookup(avltree_t *avltree, void *key) {
	avltree_node_t *node = avltree->root;
	while (node) {
		switch (avltree_compare(avltree, key, node->key)) {
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

int avltree_insert(avltree_t *avltree, avltree_node_t *node, void *key) {
	avltree_acquire_write(avltree);
	int ret = avltree_raw_insert(avltree, node, key);
	avltree_release_write(avltree);
	return ret;
}

int avltree_remove(avltree_t *avltree, void *key) {
	avltree_acquire_write(avltree);
	int ret = avltree_raw_remove(avltree, key);
	avltree_release_write(avltree);
	return ret;
}

avltree_node_t *avltree_lookup(avltree_t *avltree, void *key) {
	avltree_acquire_read(avltree);
	avltree_node_t *node = avltree_raw_lookup(avltree, key);
	avltree_release_read(avltree);
	return node;
}
