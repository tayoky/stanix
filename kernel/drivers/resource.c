#include <kernel/bus.h>
#include <kernel/irq.h>
#include <kernel/list.h>
#include <kernel/resource.h>
#include <kernel/slab.h>
#include <kernel/string.h>

// the original idea of rman comes from FreeBSD

static slab_cache_t resources_slab;
static slab_cache_t resource_descs_slab;
static slab_cache_t rman_segs_slab;

void init_resource(void) {
	slab_init(&resources_slab, sizeof(resource_t), "resources");
	slab_init(&resource_descs_slab, sizeof(resource_desc_t), "resource-descs");
	slab_init(&rman_segs_slab, sizeof(rman_seg_t), "rman-segs");
}

resource_t *resource_allocate(devnode_t *devnode, size_t start, size_t size, int flags, int rid) {
	resource_t *resource = slab_alloc(&resources_slab);
	if (!resource) return NULL;
	memset(resource, 0, sizeof(resource_t));
	resource->flags = flags;
	resource->rid   = rid;
	resource->start = start;
	resource->size  = size;
	bus_attach_resource(devnode, resource);
	return resource;
}

void resource_free(devnode_t *devnode, resource_t *resource) {
	bus_detach_resource(devnode, resource);
	slab_free(resource);
}

irq_t *resource_get_irq(resource_t *resource, size_t index) {
	if (!resource) return NULL;
	kassert((resource->flags & RESOURCE_TYPE) == RESOURCE_IRQ);
	kassert(index < resource->size);
	return irq_get_from_irqnum(main_irq_chip, resource->start + index);
}

void *resource_register_indexed_handler(resource_t *resource, size_t index, interrupt_handler_t handler, void *data) {
	return irq_register_handler(resource_get_irq(resource, index), handler, data);
}

void resource_unregister_indexed_handler(resource_t *resource, size_t index, void *handle) {
	if (!resource || IS_ERR(resource)) return;
	irq_unregister_handler(resource_get_irq(resource, index), handle);
}

resource_desc_t *resource_desc_allocate(resource_request_t *request, int rid) {
	resource_desc_t *resource_desc = slab_alloc(&resource_descs_slab);
	if (!resource_desc) return NULL;
	memset(resource_desc, 0, sizeof(resource_desc_t));
	if (request) {
		resource_desc->request = *request;
		kdebugf("allocate resource desc start=%zx size=%zu\n", request->start, request->size);
	}
	resource_desc->rid = rid;
	return resource_desc;
}

static rman_seg_t *rman_allocate_seg(size_t start, size_t size) {
	rman_seg_t *seg = slab_alloc(&rman_segs_slab);
	if (!seg) return NULL;
	memset(seg, 0, sizeof(rman_seg_t));
	seg->start = start;
	seg->size  = size;
	return seg;
}

static rman_seg_t *rman_get_seg_before(rman_t *rman, size_t addr) {
	rman_seg_t *prev = NULL;
	foreach (node, &rman->segs) {
		rman_seg_t *seg = container_of(node, rman_seg_t, node);
		if (seg->start >= addr) {
			// we are beyond
			break;
		}
		prev = seg;
	}
	return prev;
}

static rman_seg_t *rman_get_seg_at(rman_t *rman, size_t addr) {
	foreach (node, &rman->segs) {
		rman_seg_t *seg = container_of(node, rman_seg_t, node);
		if (seg->start <= addr && seg->start + seg->size > addr) {
			return seg;
		}
		if (seg->start > addr) {
			// we are beyond
			break;
		}
	}
	return NULL;
}

static int rman_seg_is_allocated(rman_seg_t *seg) {
	return seg->devnode != NULL;
}

void rman_init(rman_t *rman, int type, const char *name) {
	memset(rman, 0, sizeof(rman_t));
	mutex_init(&rman->mutex);
	rman->type = type;
	rman->name = name;
}

void rman_destroy(rman_t *rman) {
	list_node_t *node = rman->segs.first_node;
	while (node) {
		rman_seg_t *seg = container_of(node, rman_seg_t, node);
		node            = node->next;

		// ensure the seg is not in use
		kassert(!rman_seg_is_allocated(seg));

		list_remove(&rman->segs, &seg->node);
		slab_free(seg);
	}
}

static void rman_merge(rman_t *rman, rman_seg_t *seg) {
	// merge with prev
	if (seg->node.prev) {
		rman_seg_t *prev_seg = container_of(seg->node.prev, rman_seg_t, node);
		if (prev_seg->start + prev_seg->size == seg->start && !rman_seg_is_allocated(prev_seg)) {
			list_remove(&rman->segs, &prev_seg->node);
			seg->start = prev_seg->start;
			seg->size += prev_seg->size;
			slab_free(prev_seg);
		}
	}

	// merge with next
	if (seg->node.next) {
		rman_seg_t *next_seg = container_of(seg->node.next, rman_seg_t, node);
		if (seg->start + seg->size == next_seg->start && !rman_seg_is_allocated(next_seg)) {
			list_remove(&rman->segs, &next_seg->node);
			seg->size += next_seg->size;
			slab_free(next_seg);
		}
	}
}

static int rman_raw_add_region(rman_t *rman, size_t start, size_t size) {
	rman_seg_t *before = rman_get_seg_before(rman, start);
	if (before && (before->start + before->size > start)) {
		// we would overlap
		return -EINVAL;
	}

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
	rman_merge(rman, seg);
	return 0;
}

int rman_add_region(rman_t *rman, size_t start, size_t size) {
	mutex_acquire(&rman->mutex);
	int ret = rman_raw_add_region(rman, start, size);
	mutex_release(&rman->mutex);
	return ret;
}

static resource_t *rman_raw_allocate(rman_t *rman, devnode_t *devnode, resource_request_t *request) {
	// TODO : implement RESOURCE_SHARED
	// TODO : guarantee bound
	size_t start = request->start;
	size_t size  = request->size;
	size_t align = request->align;
	if (size == RESOURCE_ANY_SIZE) {
		size = 1;
	}
	if (align == RESOURCE_ANY_ALIGN) {
		align = 1;
	}

	// check align
	if (start % align != 0) {
		// unaligned
		return ERR2PTR(-EINVAL);
	}

	rman_seg_t *seg = NULL;
	foreach (node, &rman->segs) {
		rman_seg_t *cur_seg = container_of(node, rman_seg_t, node);
		if (cur_seg->start + cur_seg->size <= request->start) {
			// segment is entirely below start
			continue;
		}
		if (cur_seg->start >= request->end) {
			// segment is entirely after end
			break;
		}
		if (rman_seg_is_allocated(cur_seg)) {
			continue;
		}
		size_t seg_start = cur_seg->start;
		size_t seg_count = cur_seg->size;

		if (seg_start < request->start) {
			// strip the part of the seg before start
			seg_count -= request->start - seg_start;
			seg_start = request->start;
		}
		if (seg_start + seg_count > request->end) {
			// strip the part of the seg after end
			seg_count = request->end - seg_start;
		}
		if (seg_start % align != 0) {
			// we need to align
			size_t to_add = align - (seg_start % align);
			if (seg_count < to_add) continue;
			seg_count -= to_add;
			seg_start += to_add;
		}
		if (seg_count < size) {
			// too small
			continue;
		}

		seg   = cur_seg;
		start = seg_start;
		break;
	}

	if (!seg) {
		return ERR2PTR(-ENOMEM);
	}

	// we found a seg
	// we might need to split

	// TODO : rollback if any allocation fails
	if (seg->start < start) {
		// split before
		rman_seg_t *new_seg = rman_allocate_seg(seg->start, start - seg->start);
		if (!new_seg) return ERR2PTR(-ENOMEM);
		seg->size -= new_seg->size;
		seg->start += new_seg->size;
		list_add_before(&rman->segs, &seg->node, &new_seg->node);
	}

	if (seg->size > size) {
		// split after
		rman_seg_t *new_seg = rman_allocate_seg(start + size, seg->size - size);
		if (!new_seg) return ERR2PTR(-ENOMEM);
		seg->size -= new_seg->size;
		list_add_after(&rman->segs, &seg->node, &new_seg->node);
	}

	resource_t *resource = resource_allocate(devnode, seg->start, seg->size, request->flags | rman->type, RID_NONE);
	if (!resource) return ERR2PTR(-ENOMEM);
	resource->private = seg;

	seg->devnode = devnode;
	return resource;
}

resource_t *rman_allocate(rman_t *rman, devnode_t *devnode, resource_request_t *request) {
	mutex_acquire(&rman->mutex);
	resource_t *ret = rman_raw_allocate(rman, devnode, request);
	mutex_release(&rman->mutex);
	return ret;
}

static void rman_raw_free(rman_t *rman, devnode_t *devnode, resource_t *resource) {
	if (!resource) return;
	rman_seg_t *seg = resource->private;

	// mark seg as free
	kassert(devnode == seg->devnode);
	seg->devnode = NULL;
	
	rman_merge(rman, seg);

	resource_free(devnode, resource);
}

void rman_free(rman_t *rman, devnode_t *devnode, resource_t *resource) {
	mutex_acquire(&rman->mutex);
	rman_raw_free(rman, devnode, resource);
	mutex_release(&rman->mutex);
}
