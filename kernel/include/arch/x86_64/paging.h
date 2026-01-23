#ifndef _KERNEL_PAGING_H
#define _KERNEL_PAGING_H

#include <kernel/page.h>
#include <stdint.h>

typedef uint64_t *addrspace_t;

#define PAGING_ENTRY_ADDRESS ((~(uint64_t)0XFFF)&(~(((uint64_t)0xFFF)<<52)))

#define PAGING_FLAG_READONLY_CPL0 0x01
#define PAGING_FLAG_RW_CPL0 0x03
#define PAGING_FLAG_READONLY_CPL3 0x05
#define PAGING_FLAG_RW_CPL3 0x07
#define PAGING_FLAG_WRITE_COMBINE (1UL << 7) 
#define PAGING_FLAG_NO_EXE        (1UL << 61)

#define switch_stack() asm volatile("mov $0, %%rbp\nmov %0, %%rsp" : :)

#endif