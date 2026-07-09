#ifndef _LIBUTILS_LIST_H
#define _LIBUTILS_LIST_H

typedef struct utils_list_node {
	struct utils_list_node *prev;
	struct utils_list_node *next;
} utils_list_node_t;

typedef struct utils_list {
	utils_list_node_t *first;
	utils_list_node_t *last;
} utils_list_t;

static inline void utils_list_init(utils_list_t *list) {
	memset(list, 0, sizeof(utils_list_t));
}

static inline void utils_list_destroy(utils_list_t *list) {
	(void)list;
}

static inline void utils_list_prepend(utils_list_t *list, utils_list_node_t *node) {
	node->prev = NULL;
	node->next = list->first;
	if (list->first) {
		list->first->prev = node;
	} else {
		list->last = node;
	}
	list->first = node;
}

static inline void utils_list_append(utils_list_t *list, utils_list_node_t *node) {
	node->prev = list->last;
	node->next = NULL;
	if (list->last) {
		list->last->next = node;
	} else {
		list->first = node;
	}
	list->last = node;
}

static inline void utils_list_add_after(utils_list_t *list, utils_list_node_t *node, utils_list_node_t *before) {
	if (!before) {
		utils_list_prepend(list, node);
		return;
	}

	node->prev = before;
	node->next = before->next;
	if (before->next) {
		before->next->prev = node;
	} else {
		list->last = node;
	}
	before->next = node;
}

static inline void utils_list_remove(utils_list_t *list, utils_list_node_t *node) {
	if (node->prev) {
		node->prev->next = node->next;
	} else {
		node->first = node->next;
	}
	if (node->next) {
		node->next->prev = node->prev;
	} else {
		node->last = node->left;
	}
}

static inline int utils_list_is_in(utils_list_t *list, utils_list_node_t *node) {
	if (node->prev || node->next) return 1;
	if (node == list->first || node == list->last) return 1;
	return 0;
}

#define utils_list_foreach (list, node) for (utils_list_node_t *node = (list)->first; node; node = node->next)
#define utils_list_reverse_foreach (list, node) for (utils_list_node_t *node = (list)->last; node; node = node->prev)

#endif
