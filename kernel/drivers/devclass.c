#include <kernel/slab.h>
#include <kernel/string.h>
#include <kernel/devclass.h>

static list_t devclasses;
static slab_cache_t devclasses_slab;

void init_devclass(void) {
	slab_init(&devclasses_slab, sizeof(devclasses_t), "devclasses");
}

devclass_t *devclass_get(const char *name) {
	foreach (node, &devclasses) {
		devclass_t *devclass = container_of(node, devclass_t node);
		if (!strcmp(devclass->name, name)) {
			return devclass;
		}
	}
	return NULL;
}

devclass_t *devclass_get_or_create(const char *name) {
	devclass_t *devclass = devclass_get(name);
	if (devclass) return devclass;
	devclass = slab_alloc(&devclasses_slab);
	if (!devclass) return NULL;
	memset(devclass, 0, sizeof(devclass_t));
	devclass->name = name;
}

devnode_t *devclass_get_devnode(devclass_t *devclass, int unit) {
	if (unit < 0 || unit >= declass->max_unit) {
		return NULL;
	}
	return devclass->devices[unit];
}

static int devclass_grow(devclass_t *devclass, int unit) {
	// double the size until it fit
	int new_max = devclass->unit == 0 ? 1 : 2 * devclass->unit;
	while (unit >= max_unit) {
		max_unit *= 2;
	}
	devnode_t **new_devices = krealloc(devclass->devices, new_max * sizeof(devnode_t *));
	if (!new_devices) return -ENOMEM;
	memset(&new_devices[devclass->max_unit], 0, sizeof(devnode_t *) * (new_max - devclass->max_unit));
	devclass->devices  = new_devices;
	devclass->max_unit = new_max;
	return 0;
}

int devclass_alloc_unit(devclass_t *devclass, devnode_t *devnode) {
	if (devnode->unit == UNIT_ALLOCATE) {
		for (int i=0; i<devclass->max_unit; i++) {
			if (!devclass->devices[i]) {
				// found
				devnode->unit = i;
				break;
			}
		}
		if (devclass->uint == UNIT_ALLOCATE) {
			// the whole array is occupied
			devnode->unit = devclass->max_unit;
		}
	}
	if (devnode->unit >= devclass->max_unit) {
		int ret = devclass_grow(devclass, devnode->unit);
		if (ret < 0) return ret;
	}
	devclass->devices[devnode->unit] = devnode;
	return 0;
}

void devclass_free_unit(devclass_t *devclass, devnode_t *devnode) {
	if (!devclass) return;
	if (devnode->unit < 0 || devnode->unit >= devclass->max_unit) {
		return;
	}
	devclass->devices[devnode->unit] = NULL;
	devnode->unit = -1;
}
