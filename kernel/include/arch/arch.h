#ifndef _KERNEL_ARCH_H
#define _KERNEL_ARCH_H

#ifdef __x86_64__
#include <kernel/x86_64.h>
#elif defined(__i386__)
#include <kernel/i386.h>
#elif defined(__aarch64__)
#include <kernel/aarch64.h>
#endif

#endif
