#include <kernel/string.h>
#include <kernel/iobuf.h>
#include <kernel/mmu.h>
#include <errno.h>
	
static iobuf_pages_list_t *iobuf_pages_list_create(uintptr_t *pages, size_t pages_count) {
	iobuf_pages_list_t *pages_list = kmalloc(sizeof(iobuf_pages_list_t) + pages_count * sizeof(uintptr_t));
	if (!pages_list) return NULL;
	memset(pages_list, 0, sizeof(iobuf_pages_list_t));

	ref_count_inc(&pages_list->ref_count);
	if (pages) {
		memcpy(pages_list->pages, pages, pages_count * sizeof(uintptr_t));
	} else {
		for (size_t i = 0; i < pages_count; i++) {
			pages_list->pages[i] = PAGE_INVALID;
		}
	}
	return pages_list;
}

static iobuf_pages_list_t *iobuf_pages_list_ref(iobuf_pages_list_t *pages_list) {
	ref_count_inc(&pages_list->ref_count);
	return pages_list;
}

static void iobuf_pages_list_release(iobuf_pages_list_t *pages_list) {
	if (ref_count_dec(&pages_list->ref_count) > 1) {
		return;
	}
	kfree(pages_list);
}

int iobuf_init_pages(iobuf_t *iobuf, uintptr_t *pages, size_t offset, size_t size) {
	iobuf->type       = IOBUF_TYPE_PAGES;
	iobuf->pages_list = iobuf_pages_list_create(pages, ((offset + size) + PAGE_SIZE - 1) / PAGE_SIZE);
	if (!iobuf->pages_list) return -ENOMEM;
	iobuf->offset     = offset;
	iobuf->size       = size;
	return 0;
}

void iobuf_destroy(iobuf_t *iobuf) {
	switch (iobuf->type) {
	case IOBUF_TYPE_PAGES:
		iobuf_pages_list_release(iobuf->pages_list);
		break;
	}
	iobuf->type = IOBUF_TYPE_EMPTY;
}

int iobuf_dup(iobuf_t *dest, iobuf_t *src) {
	switch (src->type) {
	case IOBUF_TYPE_CONTINUOUS:
		dest->buffer = src->buffer;
		break;
	case IOBUF_TYPE_PAGES:
		dest->pages_list = iobuf_pages_list_ref(src->pages_list);
		break;
	default:
		kassert(!"unknown iobuf type");
		return -EINVAL;
	}
	dest->size = src->size;
	dest->type = src->type;
	return 0;
}

void *iobuf_get_at(iobuf_t *iobuf, size_t addr, size_t *size) {
	kassert(addr < iobuf->size);
	switch (iobuf->type) {
	case IOBUF_TYPE_CONTINUOUS:
		if (size) *size = iobuf->size - addr;
		return (char*)iobuf->buffer + addr;
	case IOBUF_TYPE_PAGES:;
		uintptr_t page = iobuf_get_page_at(iobuf, addr);
		if (page == PAGE_INVALID) return NULL;
		addr += iobuf->offset;
		if (size) *size = PAGE_SIZE - (addr % PAGE_SIZE);
		return mmu_phys2virt(page) + (addr % PAGE_SIZE);
	default:
		kassert(!"unknown iobuf type");
		return NULL;
	}
}

uintptr_t iobuf_get_page_at(iobuf_t *iobuf, size_t addr) {
	kassert(addr < iobuf->size);
	kassert(iobuf->type == IOBUF_TYPE_PAGES);

	return iobuf->pages_list->pages[(addr + iobuf->offset) / PAGE_SIZE];
}

static void iobuf_transfer_helper(void *iobuf_buf, void *buf, size_t size, int direction) {
	switch (direction) {
	case IOBUF_IN:
		memcpy(iobuf_buf, buf, size);
		break;
	case IOBUF_OUT:
		memcpy(buf, iobuf_buf, size);
		break;
	default:
		kassert(!"unknown iobuf direction");
		break;
	}
}

void iobuf_transfer(iobuf_t *iobuf, void *buf, size_t addr, size_t size, int direction) {
	kassert(addr < iobuf->size);
	kassert(addr + size <= iobuf->size);
	switch (iobuf->type) {
	case IOBUF_TYPE_CONTINUOUS:
		iobuf_transfer_helper((char*)iobuf->buffer + addr, buf, size, direction);
		break;
	case IOBUF_TYPE_PAGES:
		kassert(!"TODO");
		break;
	default:
		kassert(!"unknown iobuf type");
		break;
	}
}
