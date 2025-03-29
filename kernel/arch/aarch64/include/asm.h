#ifndef ASM_H
#define ASM_H

#define halt() for(;;)asm("wfi")

//TODO : actually enable/disable intterrupt
#define enable_interrupt()
#define disable_interrupt()

#endif
