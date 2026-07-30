#include <kernel/bus.h>
#include <kernel/slab.h>

static slab_cache_t resources_slab;
static bus_t *root_bus = NULL;

void bus_set_root(bus_t *bus) {
	root_bus = bus
}

bus_t *bus_get_root(void) {
	return root_bus;
}

void init_bus(void) {
	slab_init(&resources_slab, sizeof(resource_t), "resources");
}

resource_t *resource_allocate(int flags, int rid, size_t start, size_t count) {
	resource_t *resource = slab_alloc(&resources_slab);
	if (!resource) return NULL;
	memset(resource, 0, sizeof(resource_t));
	resource->flags = flags;
	resource->rid   = rid;
	resource->start = start;
	resource->count = count;
	return resource;
}
