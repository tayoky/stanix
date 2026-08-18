#ifndef KERNEL_RESOURCE_H
#define KERNEL_RESOURCE_H

#include <kernel/interrupt.h>
#include <kernel/mmio.h>
#include <kernel/list.h>
#include <kernel/port.h>
#include <kernel/mutex.h>
#include <kernel/assert.h>

struct irq;
struct devnode;

typedef struct resource_request {
	size_t start;
	size_t end;
	size_t size;
	size_t align;
	size_t bound;
	int flags;
} resource_request_t;

typedef struct resource_desc {
	list_node_t node;
	resource_request_t request;
	int rid;
} resource_desc_t;

typedef struct resource {
	list_node_t node;
	void *private;
	void *data;
	size_t start;
	size_t size;
	int flags;
	int rid;
} resource_t;

#define RESOURCE_TYPE    0xff
#define RESOURCE_IRQ     1
#define RESOURCE_IOPORT  2
#define RESOURCE_DMA     3
#define RESOURCE_MEMORY  4
//#define RESOURCE_FIXED   0x100 // allocate at specifed address
#define RESOURCE_SHARED  0x200 // allow the resource to be shared
#define RESOURCE_ACTIVE  0x400 // resource is active and usable
//#define RESOURCE_BOUND   0x800 // resource is bound (associed with the device)
//#define RESOURCE_DYNAMIC 0x800 // dynamic resource (not bound)

#define RID_ANY 0 // special rid to tell that we need any resource of the specified type (and don't care about rid)
#define RID_NONE -1
#define RESOURCE_ANY_START 0
#define RESOURCE_ANY_END   ((size_t)-1)
#define RESOURCE_ANY_SIZE  0
#define RESOURCE_ANY_ALIGN 0
#define RESOURCE_ANY_BOUND 0

typedef struct rman_seg {
	list_node_t node;
	struct devnode *devnode;
	size_t start;
	size_t size;
} rman_seg_t;

// rman is heavely inspired by freebsd
typedef struct rman {
	mutex_t mutex;
	list_t segs;
	const char *name;
	int type;
} rman_t;

void init_resource(void);

void rman_init(rman_t *rman, int type, const char *name);
void rman_destroy(rman_t *rman);
int rman_add_region(rman_t *rman, size_t start, size_t size);
resource_t *rman_allocate(rman_t *rman, struct devnode *devnode, resource_request_t *request);
void rman_free(rman_t *rman, struct devnode *devnode, resource_t *resource);

resource_t *resource_allocate(struct devnode *devnode, size_t start, size_t size, int flags, int rid);
void resource_free(struct devnode *devnode, resource_t *resource);

static inline resource_t *resource_allocate_request(struct devnode *devnode, resource_request_t *request, int rid) {
	kassert(request);
	return resource_allocate(devnode, request->start, request->size, request->flags, rid);
}

static inline resource_t *resource_allocate_data(struct devnode *devnode, void *data, size_t size, int flags, int rid) {
	return resource_allocate(devnode, (size_t)data, size, flags, rid);
}

resource_desc_t *resource_desc_allocate(resource_request_t *request, int rid);

/**
 * @brief get the virtual address of a resource
 * @param resource the resource to get the virtual address of
 * @return the virtual address
 * @note a resource might need to be activated to get a virtual address
 */
static inline void *resource_get_vaddr(resource_t *resource) {
	kassert((resource->flags & RESOURCE_TYPE) == RESOURCE_MEMORY);
	if (!resource) return NULL;
	return resource->data;
}

struct irq *resource_get_irq(resource_t *resource, size_t index);

static inline size_t resource_get_start(resource_t *resource) {
	return resource->start;
}

static inline size_t resource_get_size(resource_t *resource) {
	if (!resource) return 0;
	return resource->size;
}

void *resource_register_indexed_handler(resource_t *resource, size_t index, interrupt_handler_t handler, void *data);

void resource_unregister_indexed_handler(resource_t *resource, size_t index, void *handle);

static inline void *resource_register_handler(resource_t *resource, interrupt_handler_t handler, void *data) {
	return resource_register_indexed_handler(resource, 0, handler, data);
}

static inline void resource_unregister_handler(resource_t *resource, void *handle) {
	resource_unregister_indexed_handler(resource, 0, handle);
}

static inline uint8_t resource_read8(resource_t *resource, uint16_t index) {
	kassert(index < resource->size);
	switch (resource->flags & RESOURCE_TYPE) {
	case RESOURCE_IOPORT:
		return in_byte(resource->start + index);
	case RESOURCE_MEMORY:
		return mmio_read8(resource->data, index);
	default:
		kassert("non readable resource");
		break;
	}
}

static inline void resource_write8(resource_t *resource, uint16_t index, uint8_t data) {
	kassert(index < resource->size);
	switch (resource->flags & RESOURCE_TYPE) {
	case RESOURCE_IOPORT:
		out_byte(resource->start + index, data);
		break;
	case RESOURCE_MEMORY:
		mmio_write8(resource->data, index, data);
		break;
	default:
		kassert("non writable resource");
		break;
	}
}

static inline uint16_t resource_read16(resource_t *resource, uint16_t index) {
	kassert(index < resource->size);
	switch (resource->flags & RESOURCE_TYPE) {
	case RESOURCE_IOPORT:
		return in_word(resource->start + index);
	case RESOURCE_MEMORY:
		return mmio_read16(resource->data, index);
	default:
		kassert("non readable resource");
		break;
	}
}

static inline void resource_write16(resource_t *resource, uint16_t index, uint16_t data) {
	kassert(index < resource->size);
	switch (resource->flags & RESOURCE_TYPE) {
	case RESOURCE_IOPORT:
		out_word(resource->start + index, data);
		break;
	case RESOURCE_MEMORY:
		mmio_write16(resource->data, index, data);
		break;
	default:
		kassert("non writable resource");
		break;
	}
}

static inline uint32_t resource_read32(resource_t *resource, uint16_t index) {
	kassert(index < resource->size);
	switch (resource->flags & RESOURCE_TYPE) {
	case RESOURCE_IOPORT:
		return in_long(resource->start + index);
	case RESOURCE_MEMORY:
		return mmio_read32(resource->data, index);
	default:
		kassert("non readable resource");
		break;
	}
}

static inline void resource_write32(resource_t *resource, uint16_t index, uint32_t data) {
	kassert(index < resource->size);
	switch (resource->flags & RESOURCE_TYPE) {
	case RESOURCE_IOPORT:
		out_long(resource->start + index, data);
		break;
	case RESOURCE_MEMORY:
		mmio_write32(resource->data, index, data);
		break;
	default:
		kassert("non writable resource");
		break;
	}
}

#endif
