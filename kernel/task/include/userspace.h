#ifndef USERSPACE_H
#define USERSPACE_H

#include "page.h"

void jump_userspace(void *address,void *stack);

#define USER_STACK_TOP 0x80000000000
#define USER_STACK_SIZE 64 *PAGE_SIZE
#define USER_STACK_BOTTOM USER_STACK_TOP - USER_STACK_SIZE

#endif