#ifndef PAGE_H
#define PAGE_H

#include <limits.h> //to get PAGE_SIZE
#include <stdint.h>
#include <stdatomic.h>

#define PAGE_SHIFT  12

#define PAGE_ALIGN_DOWN(address) ((address)/PAGE_SIZE * PAGE_SIZE)
#define PAGE_ALIGN_UP(address)  (((address) + PAGE_SIZE -1)/PAGE_SIZE * PAGE_SIZE)
#define PAGE_DIV_UP(address)  (((address) + PAGE_SIZE -1)/PAGE_SIZE)

#define KERNEL_STACK_SIZE (64 * 1024)
#define USER_STACK_SIZE 64 *PAGE_SIZE
#define USER_STACK_BOTTOM USER_STACK_TOP - USER_STACK_SIZE
#define USER_STACK_TOP 0x80000000000

// memory regions
#define MEM_USERSPACE_END 0x7FFFFFFFFFFFF
#define MEM_PAGES_START   0xfffff00000000000
#define MEM_KHEAP_START   0xfffffe8000000000
#define MEM_KERNEL_START  0xffffffff80000000
#define MEM_MODULE_START  0xffffffffa0000000

#endif