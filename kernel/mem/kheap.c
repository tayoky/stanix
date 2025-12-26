#include <kernel/paging.h>
#include <kernel/kheap.h>
#include <kernel/print.h>
#include <kernel/panic.h>
#include <kernel/spinlock.h>
#include <kernel/kernel.h>
#include <kernel/string.h>

void init_kheap(void){
	kstatusf("init kheap... ");

	kernel->kheap.start = PAGE_ALIGN_DOWN(KHEAP_START);
	kernel->kheap.changes_size = change_kheap_size;

	//map a page
	map_page(get_addr_space(),pmm_allocate_page(),kernel->kheap.start,PAGING_FLAG_RW_CPL0 | PAGING_FLAG_NO_EXE);

	kernel->kheap.lenght = PAGE_SIZE;

	//init the first seg
	kernel->kheap.first_seg = (heap_segment *)kernel->kheap.start;
	kernel->kheap.first_seg->magic = HEAP_SEG_MAGIC_FREE;
	kernel->kheap.first_seg->lenght = kernel->kheap.lenght - sizeof(heap_segment);
	kernel->kheap.first_seg->prev = NULL;
	kernel->kheap.first_seg->next = NULL;

	kok();
}

void change_kheap_size(ssize_t offset){
	if(!offset)return;
	int64_t offset_page = offset / PAGE_SIZE;

	//get the addr space
	addrspace_t addr_space = get_addr_space();

	if(offset < 0){
		//make kheap smaller
		for (int64_t i = 0; i > offset_page; i--){
			uint64_t virt_page = kernel->kheap.start + kernel->kheap.lenght + i * PAGE_SIZE;
			uint64_t phys_page = (uint64_t)virt2phys((void *)virt_page);
			unmap_page(addr_space,virt_page);
			pmm_free_page(phys_page);
		}
	} else {
		//make kheap bigger
		for (int64_t i = 0; i < offset_page; i++){
			uint64_t virt_page = kernel->kheap.start + kernel->kheap.lenght + i * PAGE_SIZE;
			map_page(addr_space,pmm_allocate_page(),virt_page, PAGING_FLAG_RW_CPL0 | PAGING_FLAG_NO_EXE);
		}
	}
	kernel->kheap.lenght += offset_page * PAGE_SIZE;
}

void show_memseg(){
	heap_segment *current_seg = kernel->kheap.first_seg;
	char *labels[] ={
		"free     ",
		"allocated"
	};
	while (current_seg){
		kdebugf("memseg %s start : 0x%lx lenght : 0x%lx\n",labels[(current_seg->magic == HEAP_SEG_MAGIC_ALLOCATED)],current_seg,current_seg->lenght);
		current_seg = current_seg->next;
	}
}

void *malloc(heap_info *heap,size_t amount){
	//make amount aligned
	if(amount & 0b1111)
	amount += 16 - (amount % 16);
	
	spinlock_acquire(&heap->lock);
	heap_segment *current_seg = heap->first_seg;
	while(current_seg->lenght < amount || current_seg->magic != HEAP_SEG_MAGIC_FREE){
		if(current_seg->next == NULL){
			//no more segment need to make heap bigger
			if(!heap->changes_size){
				//fixed size heap
				return NULL;
			}

			//if last is free make it bigger else create a new seg from scratch
			if(current_seg->magic == HEAP_SEG_MAGIC_FREE){
				heap->changes_size(PAGE_ALIGN_UP(amount - current_seg->lenght + sizeof(heap_segment) + 16));
				current_seg->lenght += PAGE_ALIGN_UP(amount - current_seg->lenght + sizeof(heap_segment) + 16);
			} else {
				heap->changes_size(PAGE_ALIGN_UP(amount + sizeof(heap_segment) * 2  + 16));
				heap_segment *new_seg = (heap_segment *)((uintptr_t)current_seg + current_seg->lenght + sizeof(heap_segment));
				new_seg->lenght = PAGE_ALIGN_UP(amount + sizeof(heap_segment) * 2 + 16) - sizeof(heap_segment);
				new_seg->magic = HEAP_SEG_MAGIC_FREE;
				new_seg->next = NULL;
				new_seg->prev = current_seg;
				current_seg->next = new_seg;
				current_seg = current_seg->next;
			}
			break;
		}
		if((uintptr_t)current_seg->next % 16){
			kdebugf("found non aligned kheap seg at %p after %p(%p:%ld)\n",current_seg->next,current_seg,(uintptr_t)current_seg + sizeof(heap_segment),current_seg->lenght);
		}
		current_seg = current_seg->next;
	}

	//we find a good segment

	//if big enougth cut it
	if(current_seg->lenght >= amount + sizeof(heap_segment) + 16){
		//big enougth
		heap_segment *new_seg = (heap_segment *)((uintptr_t)current_seg + amount + sizeof(heap_segment));
		new_seg->lenght = current_seg->lenght - (sizeof(heap_segment) + amount);
		current_seg->lenght = amount;
		new_seg->magic = HEAP_SEG_MAGIC_FREE;

		if(current_seg->next){
			current_seg->next->prev = new_seg;
		}
		new_seg->next = current_seg->next;
		new_seg->prev = current_seg;
		current_seg->next = new_seg;
	}

	current_seg->magic = HEAP_SEG_MAGIC_ALLOCATED;
	spinlock_release(&heap->lock);
	return (char *)current_seg + sizeof(heap_segment);
}

void free(heap_info *heap,void *ptr){
	if(!ptr)return;

	spinlock_acquire(&heap->lock);
	
	heap_segment *current_seg = (heap_segment *)((uintptr_t)ptr - sizeof(heap_segment));
	if(current_seg->magic != HEAP_SEG_MAGIC_ALLOCATED){
		if(current_seg->magic == HEAP_SEG_MAGIC_FREE){
			kdebugf("try to free aready free segement\n");
			spinlock_release(&heap->lock);
			return;
		}
		kdebugf("try to free not allocated heap seg at %p\n",current_seg);
		panic("heap error",NULL);
		spinlock_release(&heap->lock);
		return;
	}

	current_seg->magic = HEAP_SEG_MAGIC_FREE;

	if(current_seg->next && current_seg->next->magic == HEAP_SEG_MAGIC_FREE){
		//merge with next
		current_seg->lenght += current_seg->next->lenght + sizeof(heap_segment);

		if(current_seg->next->next){
			current_seg->next->next->prev = current_seg;
		}
		current_seg->next = current_seg->next->next;
	}

	if(current_seg->prev && current_seg->prev->magic == HEAP_SEG_MAGIC_FREE){
		//merge with prev
		current_seg->prev->lenght += current_seg->lenght + sizeof(heap_segment);

		if(current_seg->next){
			current_seg->next->prev = current_seg->prev;
		}
		current_seg->prev->next = current_seg->next;
	}
	spinlock_release(&heap->lock);
}

void *kmalloc(size_t amount){
	return malloc(&kernel->kheap,amount);
}

void kfree(void *ptr){
	return free(&kernel->kheap,ptr);
}


void *krealloc(void *ptr, size_t new_size) {
	if(!ptr){
		return kmalloc(new_size);
	}
	if(!new_size){
		kfree(ptr);
		return NULL;
	}
	heap_segment *seg = (heap_segment *)((uintptr_t)ptr - sizeof(heap_segment));
	void *new_buf = kmalloc(new_size);
	if(!new_buf){
		return NULL;
	}

	if(new_size > seg->lenght){
		memcpy(new_buf,ptr,seg->lenght);
	} else {
		memcpy(new_buf,ptr,seg->lenght);
	}
	kfree(ptr);
	return new_buf;
}
