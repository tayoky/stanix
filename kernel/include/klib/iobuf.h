#ifndef KERNEL_IOBUF_H
#define KERNEL_IOBUF_H

#include <kernel/page.h>
#include <kernel/refcount.h>
#include <kernel/assert.h>
#include <kernel/kheap.h>

typedef struct iobuf_pages_list {
	ref_count_t ref_count;
	uintptr_t pages[];
} iobuf_pages_list_t;

typedef struct iobuf {
	union {
		void *buffer;
		iobuf_pages_list_t *pages_list;
	};
	size_t size;
	size_t offset;
	int type;
} iobuf_t;

#define IOBUF_TYPE_EMPTY      0x0
#define IOBUF_TYPE_CONTINUOUS 0x1
#define IOBUF_TYPE_PAGES      0x2

static inline void iobuf_init_continuous(iobuf_t *iobuf, void *buffer, size_t size) {
	iobuf->type   = IOBUF_TYPE_CONTINUOUS;
	iobuf->buffer = buffer;
	iobuf->size   = size;
	iobuf->offset = 0;
}

int iobuf_init_pages(iobuf_t *iobuf, uintptr_t *pages, size_t offset, size_t size);

static inline void iobuf_set_page(iobuf_t *iobuf, size_t addr, uintptr_t page) {
	kassert(iobuf->type == IOBUF_TYPE_PAGES);
	addr += iobuf->offset;
	kassert(addr % PAGE_SIZE == 0);
	iobuf->pages_list->pages[addr / PAGE_SIZE] = page;
}

void iobuf_destroy(iobuf_t *iobuf);
int iobuf_dup(iobuf_t *dest, iobuf_t *src);

/**
 * @brief get a continuous buffer
 * @param iobuf the iobuf to get a continuous buffer to
 * @param addr the address into the iobuf to get the buffer to
 * @param size the size of the continuous buffer / hole
 * @return the continuous buffer on success or else NULL if addr point to a hole
 */
void *iobuf_get_at(iobuf_t *iobuf, size_t addr, size_t *size);

uintptr_t iobuf_get_page_at(iobuf_t *iobuf, size_t addr);

void iobuf_transfer(iobuf_t *iobuf, void *buf, size_t addr, size_t size, int direction);
#define IOBUF_IN  1
#define IOBUF_OUT 2

#endif
