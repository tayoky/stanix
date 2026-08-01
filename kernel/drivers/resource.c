#include <kernel/list.h>
#include <kernel/slab.h>
#include <kernel/bus.h>
#include <kernel/resource.h>
#include <kernel/string.h>

static slab_cache_t resources_slab;
static slab_cache_t resource_descs_slab;
static slab_cache_t rman_segs_slab;

void init_resource(void) {
	slab_init(&resources_slab, sizeof(resource_t), "resources");
	slab_init(&resource_descs_slab, sizeof(resource_desc_t), "resource-descs");
	slab_init(&rman_segs_slab, sizeof(rman_seg_t), "rman-segs");
}

resource_t *resource_allocate(int flags, int rid, size_t start, size_t size) {
	resource_t *resource = slab_alloc(&resources_slab);
	if (!resource) return NULL;
	memset(resource, 0, sizeof(resource_t));
	resource->flags = flags;
	resource->rid   = rid;
	resource->start = start;
	resource->size  = size;
	return resource;
}

resource_desc_t *resource_desc_allocate(int flags, int rid, size_t start, size_t size) {
	resource_desc_t *resource_desc = slab_alloc(&resource_descs_slab);
	if (!resource_desc) return NULL;
	memset(resource_desc, 0, sizeof(resource_desc_t));
	resource_desc->flags = flags;
	resource_desc->rid   = rid;
	resource_desc->start = start;
	resource_desc->size  = size;
	return resource_desc;
}

static rman_seg_t *rman_allocate_seg(size_t start, size_t size) {
	rman_seg_t *seg = slab_alloc(&rman_segs_slab);
	if (!seg) return NULL;
	memset(seg, 0, sizeof(rman_seg_t));
	seg->start = start;
	seg->size = size;
	return seg;
}

static rman_seg_t *rman_get_seg_before(rman_t *rman, size_t addr) {
	rman_seg_t *prev = NULL;
	foreach(node, &rman->segs) {
		rman_seg_t *seg = container_of(node, rman_seg_t, node);
		if (seg->start >= addr) {
			// we are beyond
			break;
		}
	}
	return prev;
}

static rman_seg_t *rman_get_seg_at(rman_t *rman, size_t addr) {
	rman_seg_t *prev = NULL;
	foreach(node, &rman->segs) {
		rman_seg_t *seg = container_of(node, rman_seg_t, node);
		if (seg->start > addr) {
			// we are beyond
			break;
		}
	}
	return prev;
}

static int rman_seg_is_allocated(rman_seg_t *seg) {
	return seg->devnode != NULL;
}

void rman_init(rman_t *rman, int type, const char *name) {
	memset(rman, 0, sizeof(rman_t));
	rman->type = type;
	rman->name = name;
}

void rman_destroy(rman_t *rman) {
	kwarningf("TODO : rman_destroy\n");
}

int rman_add_region(rman_t *rman, size_t start, size_t size) {
	rman_seg_t *before = rman_get_seg_before(rman, start);

	if (!list_is_empty(&rman->segs)) {
		// check if we would overlap
		list_node_t *after_node = before ? before->node.next : rman->segs.first_node;
		if (after_node) {
			rman_seg_t *after = container_of(after_node, rman_seg_t, node);
			if (after->start < start + size) {
				// we would overlap
				return -EINVAL;
			}
		}
	}
	rman_seg_t *seg = rman_allocate_seg(start, size);
	if (!seg) return -ENOMEM;
	list_add_after(&rman->segs, before ? &before->node : NULL, &seg->node);
	return 0;
}

void rman_set_dynamic_start(rman_t *rman, size_t start) {
	rman->dynamic_start = start;
}

resource_t *rman_allocate(rman_t *rman, devnode_t *devnode, size_t start, size_t size, int flags) {
	// TODO : implement RESOURCE_SHARED
	if (size == RESOURCE_ANY_SIZE) {
		size = 1;
	}

	rman_seg_t *seg = NULL;
	if (start == RESOURCE_ANY_START) {
		foreach(node, &rman->segs) {
			rman_seg_t *cur_seg = container_of(node, rman_seg_t, node);
			if (cur_seg->start + cur_seg->size <= rman->dynamic_start) {
				// below dynamic start
				continue;
			}
			size_t seg_count = cur_seg->size;
			if (cur_seg->start < rman->dynamic_start) {
				// not the whole seg can be allocated
				seg_count -= rman->dynamic_start - cur_seg->start;
			}
			if (!rman_seg_is_allocated(cur_seg) && seg_count >= size) {
				seg = cur_seg;
				if (seg->start < rman->dynamic_start) {
					start = rman->dynamic_start;
				} else {
					start = seg->start;
				}
			}
		}
	} else {
		seg = rman_get_seg_at(rman, start);
		if (!seg || rman_seg_is_allocated(seg) || seg->start + seg->size < start + size) {
			return ERR2PTR(-ENOMEM);
		}
	}

	// we found a seg
	// we might need to split
	
	if (seg->start < start) {
		// split before
		rman_seg_t *new_seg = rman_allocate_seg(seg->start, start - seg->start);
		if (!seg) return ERR2PTR(-ENOMEM);
		seg->size -= new_seg->size;
		seg->start += new_seg->size;
		list_add_before(&rman->segs, &seg->node, &new_seg->node);
	}

	if (seg->size > size) {
		// split after
		rman_seg_t *new_seg = rman_allocate_seg(start + size, seg->size - size);
		if (!seg) return ERR2PTR(-ENOMEM);
		seg->size -= new_seg->size;
		list_add_after(&rman->segs, &seg->node, &new_seg->node);
	}


	resource_t *resource = resource_allocate(RID_ANY, flags | rman->type, seg->start, seg->size);
	if (!resource) return ERR2PTR(-ENOMEM);

	seg->devnode = devnode;
	return resource;
}

void rman_free(rman_t *rman, resource_t *resource) {
	kwarningf("TODO : rman_free\n");
}
