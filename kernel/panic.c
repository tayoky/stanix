#include "panic.h"
#include "print.h"
#include "asm.h"

void register_dump(regs *registers){
    asm("mov %%rax, %0" : "=r"(registers->rax):);
    asm("mov %%rbx, %0" : "=r"(registers->rbx):);
    asm("mov %%rcx, %0" : "=r"(registers->rcx):);
    asm("mov %%rdx, %0" : "=r"(registers->rdx):);
    asm("mov %%rsi, %0" : "=r"(registers->rsi):);
    asm("mov %%rdi, %0" : "=r"(registers->rdi):);
    asm("mov %%r8 , %0" : "=r"(registers->r8):);
    asm("mov %%r9 , %0" : "=r"(registers->r9):);
    asm("mov %%r10, %0" : "=r"(registers->r10):);
    asm("mov %%r11, %0" : "=r"(registers->r11):);
    asm("mov %%r12, %0" : "=r"(registers->r12):);
    asm("mov %%r13, %0" : "=r"(registers->r13):);
    asm("mov %%r14, %0" : "=r"(registers->r14):);
    asm("mov %%r15, %0" : "=r"(registers->r15):);
}

void panic(const char *error,regs registers_state){
    if(!registers_state.cr2)register_dump(&registers_state);
    kprintf("====== ERROR : KERNEL PANIC =====\n");
    kprintf("error : %s\n",error);
    kprintf("========== REGISTER DUMP ========\n");
    kprintf("rax : 0x%x\tr8  : 0x%x\n",registers_state.rax,registers_state.r8);
    kprintf("rbx : 0x%x\tr9  : 0x%x\n",registers_state.rbx,registers_state.r9);
    kprintf("rcx : 0x%x\tr10 : 0x%x\n",registers_state.rcx,registers_state.r10);
    kprintf("rdx : 0x%x\tr11 : 0x%x\n",registers_state.rdx,registers_state.r11);
    kprintf("rsi : 0x%x\tr12 : 0x%x\n",registers_state.rsi,registers_state.r12);
    kprintf("rdi : 0x%x\tr13 : 0x%x\n",registers_state.rdi,registers_state.r13);
    kprintf("rbp : 0x%x\tr14 : 0x%x\n",registers_state.rbp,registers_state.r14);
    kprintf("rip : 0x%x\tr15 : 0x%x\n",registers_state.rip,registers_state.r15);
    kprintf("======= SPECIAL REGISTERS =======\n");
    asm("mov %%cr2 ,%%rax" : "=a"(registers_state.cr2) : );
    asm("mov %%cr3 ,%%rax" : "=a"(registers_state.cr3) : );
    kprintf("cr2 : 0x%x\tcr3 : 0x%x\n",registers_state.cr2,registers_state.cr3);
    halt();
}