#include <kernel/kernel.h>
#include <kernel/kheap.h>
#include <kernel/mmu.h>
#include <kernel/panic.h>
#include <kernel/pmm.h>
#include <kernel/print.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>

#define KHEAP_MMU_FLAGS MMU_FLAG_READ | MMU_FLAG_WRITE | MMU_FLAG_PRESENT | MMU_FLAG_GLOBAL


static uintptr_t kheap_start;
static uintptr_t kheap_length;
static spinlock_t kheap_lock;
static heap_segment_t *first_seg;

void init_kheap(void) {
	kstatusf("init kheap... ");

	kheap_start = PAGE_ALIGN_DOWN(MEM_KHEAP_START);

	// map a page
	mmu_map_page(mmu_get_addr_space(), pmm_allocate_page(), kheap_start, KHEAP_MMU_FLAGS);

	kheap_length = PAGE_SIZE;

	// init the first seg
	first_seg         = (heap_segment_t *)kheap_start;
	first_seg->magic  = HEAP_SEG_MAGIC_FREE;
	first_seg->length = kheap_length - sizeof(heap_segment_t);
	first_seg->prev   = NULL;
	first_seg->next   = NULL;

	kok();
}

static void change_kheap_size(ssize_t offset) {
	if (!offset) return;
	intptr_t offset_page = offset / PAGE_SIZE;

	// get the addr space
	addrspace_t addr_space = mmu_get_addr_space();

	if (offset < 0) {
		// make kheap smaller
		for (intptr_t i = 0; i > offset_page; i--) {
			uintptr_t virt_page = kheap_start + kheap_length + i * PAGE_SIZE;
			uintptr_t phys_page = mmu_virt2phys((void *)virt_page);
			mmu_unmap_page(addr_space, virt_page);
			pmm_release_page(phys_page);
		}
	} else {
		// make kheap bigger
		for (intptr_t i = 0; i < offset_page; i++) {
			uintptr_t virt_page = kheap_start + kheap_length + i * PAGE_SIZE;
			mmu_map_page(addr_space, pmm_allocate_page(), virt_page, KHEAP_MMU_FLAGS);
		}
	}
	kheap_length += offset_page * PAGE_SIZE;
}

static void show_memsegs(void) {
	heap_segment_t *current_seg = first_seg;
	char *labels[]              = {
		"free     ",
		"allocated"};
	while (current_seg) {
		kdebugf("vmm_seg %s kheap_start : 0x%lx kheap_length : 0x%lx\n", labels[(current_seg->magic == HEAP_SEG_MAGIC_ALLOCATED)], current_seg, current_seg->length);
		current_seg = current_seg->next;
	}
}

void *malloc(size_t amount) {
	// make amount aligned
	if (amount & 0b1111) {
		amount += 16 - (amount % 16);
	}

	spinlock_acquire(&kheap_lock);
	heap_segment_t *current_seg = first_seg;
	while (current_seg->length < amount || current_seg->magic != HEAP_SEG_MAGIC_FREE) {
		if (current_seg->next == NULL) {
			// no more segment need to make heap bigger

			// if last is free make it bigger else create a new seg from scratch
			if (current_seg->magic == HEAP_SEG_MAGIC_FREE) {
				change_kheap_size(PAGE_ALIGN_UP(amount - current_seg->length + sizeof(heap_segment_t) + 16));
				current_seg->length += PAGE_ALIGN_UP(amount - current_seg->length + sizeof(heap_segment_t) + 16);
			} else {
				change_kheap_size(PAGE_ALIGN_UP(amount + sizeof(heap_segment_t) * 2 + 16));
				heap_segment_t *new_seg = (heap_segment_t *)((uintptr_t)current_seg + current_seg->length + sizeof(heap_segment_t));
				new_seg->length         = PAGE_ALIGN_UP(amount + sizeof(heap_segment_t) * 2 + 16) - sizeof(heap_segment_t);
				new_seg->magic          = HEAP_SEG_MAGIC_FREE;
				new_seg->next           = NULL;
				new_seg->prev           = current_seg;
				current_seg->next       = new_seg;
				current_seg             = current_seg->next;
			}
			break;
		}
		if ((uintptr_t)current_seg->next % 16) {
			kdebugf("found non aligned kheap seg at %p after %p(%p:%ld)\n", current_seg->next, current_seg, (uintptr_t)current_seg + sizeof(heap_segment_t), current_seg->length);
		}
		current_seg = current_seg->next;
	}

	// we find a good segment

	// if big enougth cut it
	if (current_seg->length >= amount + sizeof(heap_segment_t) + 16) {
		// big enougth
		heap_segment_t *new_seg = (heap_segment_t *)((uintptr_t)current_seg + amount + sizeof(heap_segment_t));
		new_seg->length         = current_seg->length - (sizeof(heap_segment_t) + amount);
		current_seg->length     = amount;
		new_seg->magic          = HEAP_SEG_MAGIC_FREE;

		if (current_seg->next) {
			current_seg->next->prev = new_seg;
		}
		new_seg->next     = current_seg->next;
		new_seg->prev     = current_seg;
		current_seg->next = new_seg;
	}

	current_seg->magic = HEAP_SEG_MAGIC_ALLOCATED;
	spinlock_release(&kheap_lock);
	return (char *)current_seg + sizeof(heap_segment_t);
}

void free(void *ptr) {
	if (!ptr) return;

	spinlock_acquire(&kheap_lock);

	heap_segment_t *current_seg = (heap_segment_t *)((uintptr_t)ptr - sizeof(heap_segment_t));
	if (current_seg->magic != HEAP_SEG_MAGIC_ALLOCATED) {
		if (current_seg->magic == HEAP_SEG_MAGIC_FREE) {
			kdebugf("try to free aready free segement\n");
			spinlock_release(&kheap_lock);
			return;
		}
		kdebugf("try to free not allocated heap seg at %p\n", current_seg);
		show_memsegs();
		panic("heap error", NULL);
		spinlock_release(&kheap_lock);
		return;
	}

	current_seg->magic = HEAP_SEG_MAGIC_FREE;

	if (current_seg->next && current_seg->next->magic == HEAP_SEG_MAGIC_FREE) {
		// merge with next
		current_seg->length += current_seg->next->length + sizeof(heap_segment_t);

		if (current_seg->next->next) {
			current_seg->next->next->prev = current_seg;
		}
		current_seg->next = current_seg->next->next;
	}

	if (current_seg->prev && current_seg->prev->magic == HEAP_SEG_MAGIC_FREE) {
		// merge with prev
		current_seg->prev->length += current_seg->length + sizeof(heap_segment_t);

		if (current_seg->next) {
			current_seg->next->prev = current_seg->prev;
		}
		current_seg->prev->next = current_seg->next;
	}
	spinlock_release(&kheap_lock);
}

void *kmalloc(size_t amount) {
	return malloc(amount);
}

void kfree(void *ptr) {
	return free(ptr);
}


void *krealloc(void *ptr, size_t new_size) {
	if (!ptr) {
		return kmalloc(new_size);
	}
	if (!new_size) {
		kfree(ptr);
		return NULL;
	}
	heap_segment_t *seg = (heap_segment_t *)((uintptr_t)ptr - sizeof(heap_segment_t));
	void *new_buf       = kmalloc(new_size);
	if (!new_buf) {
		return NULL;
	}

	if (new_size > seg->length) {
		memcpy(new_buf, ptr, seg->length);
	} else {
		memcpy(new_buf, ptr, new_size);
	}
	kfree(ptr);
	return new_buf;
}
