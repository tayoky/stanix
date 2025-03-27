#ifndef ASM_H
#define ASM_H

#define disable_interrupt() asm("cli")
#define enable_interrupt() asm("sti")
#define halt() while(1)
#endif