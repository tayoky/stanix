#ifndef KERNEL_RESOURCE_H
#define KERNEL_RESOURCE_H

#include <kernel/interrupt.h>
#include <kernel/list.h>
#include <kernel/port.h>

typedef struct resource {
	list_node_t node;
	void *private;
	void *vaddr;
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
#define RESOURCE_FIXED   0x100 // allocate at specifed address
#define RESOURCE_SHARED  0x200 // allow the resource to be shared
#define RESOURCE_ACTIVE  0x400 // resource is active and usable
#define RESOURCE_BOUND   0x800 // resource is bound (associed with the device)
//#define RESOURCE_DYNAMIC 0x800 // dynamic resource (not bound)

#define RID_DYNAMIC 0 // the resource does not fill any specific rid

resource_t *resource_allocate(int flags, int rid, size_t start, size_t count);

/**
 * @brief get the virtual address of a resource
 * @param resource the resource to get the virtual address of
 * @return the virtual address
 * @note a resource might need to be activated to get a virtual address
 */
static inline void *resource_get_vaddr(resource_t *resource) {
	if (!resource) return NULL;
	return resource->vaddr;
}

static inline size_t resource_get_start(resource_t *resource) {
	return resource->start;
}

static inline size_t resource_get_size(resource_t *resource) {
	if (!resource) return 0;
	return resource->size;
}

static inline uint8_t resource_in_byte(resource_t *resource, uint16_t ìndex) {
	kassert(resource->flags & RESOURCE_TYPE == RESOURCE_IOPORT);
	kassert(index < resource->size);
	return in_byte(resource->start + index);
}

static inline void resource_out_byte(resource_t *resource, uint16_t index, uint8_t data) {
	kassert(resource->flags & RESOURCE_TYPE == RESOURCE_IOPORT);
	kassert(index < resource->size);
	out_byte(resource->start + index, data);
}

static inline uint16_t resource_in_word(resource_t *resource, uint16_t ìndex) {
	kassert(resource->flags & RESOURCE_TYPE == RESOURCE_IOPORT);
	kassert(index < resource->size);
	return in_word(resource->start + index);
}

static inline void resource_out_word(resource_t *resource, uint16_t index, uint16_t data) {
	kassert(resource->flags & RESOURCE_TYPE == RESOURCE_IOPORT);
	kassert(index < resource->size);
	out_word(resource->start + index, data);
}

static inline uint32_t resource_in_long(resource_t *resource, uint16_t ìndex) {
	kassert(resource->flags & RESOURCE_TYPE == RESOURCE_IOPORT);
	kassert(index < resource->size);
	return in_long(resource->start + index);
}

static inline void resource_out_long(resource_t *resource, uint16_t index, uint32_t data) {
	kassert(resource->flags & RESOURCE_TYPE == RESOURCE_IOPORT);
	kassert(index < resource->size);
	out_long(resource->start + index, data);
}

#endif
