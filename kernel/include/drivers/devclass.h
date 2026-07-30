#ifndef KERNEL_DEVCLASS_H
#define KERNEL_DEVCLASS_H

#include <kernel/list.h>

struct devnode;

typedef struct devclass {
	list_node_t node;
	const char *name;
	int max_unit;
	struct devnode **devices;
} devclass_t;

/**
 * @brief get a devclass by name
 * @param name the name of the devclass
 * @return the devclass if it exist or NULL
 */
devclass_t *devclass_get(const char *name);

devclass_t *devclass_get_or_create(const char *name);
struct devnode *devclass_get_devnode(devclass_t *devclass, int unit);
int devclass_alloc_unit(devclass_t *devclass, struct devnode *devnode);
void devclass_free_unit(devclass_t *devclass, struct devnode *devnode);
void init_devclass(void);

#define UNIT_ALLOCATE -2

#endif
