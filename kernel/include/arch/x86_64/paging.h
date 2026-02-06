#ifndef _KERNEL_PAGING_H
#define _KERNEL_PAGING_H

#include <kernel/page.h>
#include <stdint.h>

typedef uint64_t *addrspace_t;

#define PAGING_ENTRY_ADDRESS ((~(uint64_t)0XFFF)&(~(((uint64_t)0xFFF)<<52)))

#define PAGING_FLAG_PRESENT 0x01
#define PAGING_FLAG_WRITE   0x02
#define PAGING_FLAG_USER    0x04
#define PAGING_FLAG_PWT     0x08
#define PAGING_FLAG_PCD     0x10
#define PAGING_FLAG_ACCESS  0x20
#define PAGING_FLAG_DIRTY   0x40
#define PAGING_FLAG_NO_EXE  (1UL << 61)

#define PAGING_FLAG_READONLY_CPL0  PAGING_FLAG_PRESENT
#define PAGING_FLAG_RW_CPL0        PAGING_FLAG_READONLY_CPL0 | PAGING_FLAG_WRITE
#define PAGING_FLAG_READONLY_CPL3  PAGING_FLAG_PRESENT | PAGING_FLAG_USER
#define PAGING_FLAG_RW_CPL3        PAGING_FLAG_READONLY_CPL3 | PAGING_FLAG_WRITE
#define PAGING_FLAG_WRITE_COMBINE (1UL << 7) 

#define switch_stack() asm volatile("mov $0, %%rbp\nmov %0, %%rsp" : :)

#endif