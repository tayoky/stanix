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
	right->parent = node->parent;
	node->parent = right;
	if (node->right) node->right->parent = node;

	avltree_compute_height(avltree, node);
	avltree_compute_height(avltree, right);
	return right;
}

static avltree_node_t *avltree_rotate_right(avltree_t *avltree, avltree_node_t *node) {
	avltree_node_t *left = node->left;
	node->left = left->right;
	left->right = node;
	left->parent = node->parent;
	node->parent = left;
	if (node->left) node->left->parent = node;

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

static void avltree_insert_helper(avltree_t *avltree, avltree_node_t *parent, avltree_node_t **current, avltree_node_t *node) {
	if (!*current) {
		node->parent = parent;
		*current = node;
		return;
	}

	avltree_node_t **child;
	switch (avltree_compare_nodes(avltree, node, *current)) {
	case -1:
		child = &(*current)->left;
		break;
	case 0: // if equal also go to the right
	case 1:
		child = &(*current)->right;
		break;
	default:
		__builtin_unreachable();
	}
	
	avltree_insert_helper(avltree, *current, child, node);
	avltree_rebalance(avltree, current, child);
}

void avltree_insert(avltree_t *avltree, avltree_node_t *node) {
	node->height = 1;

	avltree_acquire_write(avltree);
	avltree_insert_helper(avltree, NULL, &avltree->root, node);
	avltree_release_write(avltree);
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
	case 0:
		if (*current != node) {
			// the node we are searching is equal to this one
			// so go right
			child = &(*current)->right;
			break;
		}
		// this is the node to delete
		avltree_node_t *cur_node = *current;
		if (!cur_node->right) {
			// no children or only left children
			*current = cur_node->left;
			if (cur_node->left) cur_node->left->parent = cur_node->parent;
		} else if (!cur_node->left) {
			// only right children
			*current = cur_node->right;
			if (cur_node->right) cur_node->right->parent = cur_node->parent;
		} else {
			// both left and right children
			avltree_node_t *leftest = avltree_leftest_subnode(cur_node->right);

			// remove the leftest subnode of right
			if (!avltree_remove_helper(avltree, &cur_node->right, leftest)) {
				// normally we must find the leftest node
				__builtin_unreachable();
			}

			// and replace it over us
			leftest->left   = cur_node->left;
			leftest->right  = cur_node->right;
			leftest->parent = cur_node->parent;
			if (leftest->left)  leftest->left->parent = leftest;
			if (leftest->right) leftest->right->parent = leftest;
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
 
int avltree_remove(avltree_t *avltree, avltree_node_t *node) {
	avltree_acquire_write(avltree);
	int ret = avltree_remove_helper(avltree, &avltree->root, node);
	avltree_release_write(avltree);
	return ret;
}

avltree_node_t *avltree_node_lookup(avltree_t *avltree, avltree_node_t *node, void *key) {
	avltree_acquire_read(avltree);
	while (node) {
		switch (avltree_compare_key(avltree, key, node)) {
		case -1:
			node = node->left;
			break;
		case 1:
			node = node->right;
			break;
		case 0:
			avltree_release_read(avltree);
			return node;
		default:
			__builtin_unreachable();
		}
	}
	avltree_release_read(avltree);
	return NULL;
}

avltree_node_t *avltree_lookup(avltree_t *avltree, void *key) {
	avltree_acquire_read(avltree);
	avltree_node_t *node = avltree_node_lookup(avltree, avltree->root, key);
	avltree_release_read(avltree);
	return node;
}

avltree_node_t *avltree_first(avltree_t *avltree) {
	avltree_acquire_read(avltree);
	avltree_node_t *node = avltree->root;
	while (node && node->left) {
		node = node->left;
	}
	avltree_release_read(avltree);
	return node;
}

avltree_node_t *avltree_next(avltree_t *avltree, avltree_node_t *node) {
	if (!node) return NULL;
	avltree_acquire_read(avltree);

	// we already visisted the current node and node->left
	// we want to go right or up
	if (node->right) {
		// we can go into the right subtree
		avltree_node_t *right = avltree_leftest_subnode(node->right);
		avltree_release_read(avltree);
		return right;
	}

	// move up
	avltree_node_t *parent = node->parent;
	while (parent && parent->right == node) {
		node = parent;
		parent = node->parent;
	}
	avltree_release_read(avltree);
	return parent;
}

avltree_node_t *avltree_next_key(avltree_t *avltree, avltree_node_t *node, void *key) {
	if (!node) return NULL;
	avltree_acquire_read(avltree);

	// we already visisted the current node and node->left
	// we want to go right or up
	if (node->right) {
		// can we go into the right subtree ?
		avltree_node_t *right_match = avltree_node_lookup(avltree, node->right, key);
		if (right_match) {
			// yes we can
			avltree_release_read(avltree);
			return right_match;
		}
	}

	// move up
	avltree_node_t *parent = node->parent;
	while (parent) {
		if (parent->left == node) { 
			// we are the left subtree
			// which mean we didn't visit the parent and its right subtree
			if (avltree_compare_key(avltree, key, parent) == 0) {
				// the parent match
				break;
			} else if (parent->right) {
				// can we go into the right subtree ?
				avltree_node_t *right_match = avltree_node_lookup(avltree, parent->right, key);
				if (right_match) {
					// yes we can
					avltree_release_read(avltree);
					return right_match;
				}
			}
		}
		node = parent;
		parent = node->parent;
	}
	avltree_release_read(avltree);
	return parent;
}
