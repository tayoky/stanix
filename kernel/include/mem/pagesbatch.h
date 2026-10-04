#ifndef KERNEL_PAGESBATCH_H
#define KERNEL_PAGESBATCH_H

#include <kernel/pmm.h>

#define PAGES_PER_BATCH 31

typedef struct pages_batch {
	size_t pages_count;
	uintptr_t pages[PAGES_PER_BATCH];
} pages_batch_t;

static inline void pages_batch_init(pages_batch_t *pages_batch) {
	pages_batch->pages_count = 0;
}

static inline pages_batch_is_empty(pages_batch_t *pages_batch) {
	return pages_batch->pages_count == 0;
}

static inline pages_batch_is_full(pages_batch_t *pages_batch) {
	return pages_batch->pages_count >= PAGES_PER_BATCH;
}

static inline int pages_batch_add(pages_batch_t *pages_batch, uintptr_t page) {
	if (pages_batch->pages_count >= PAGES_PER_BATCH) {
		return 0;
	}
	pages_batch->pages[pages_batch->pages_count++] = page;
	return 1;
}

static inline void pages_batch_from_page(pages_batch_t *pages_batch, uintptr_t page) {
	pages_batch->pages_count = 1;
	pages_batch->pages[0] = page;
}

#define pages_batch_foreach(page, pages_batch) for (uintptr_t i = 0, page = (pages_batch)->pages[0]; i < (pages_batch)->pages_count; i++, page = (i < (pages_batch)->pages_count ? (pages_batch)->pages[i] : PAGE_INVALID))

#endif
