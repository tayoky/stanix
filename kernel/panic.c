#include "panic.h"
#include "print.h"
#include "asm.h"

void register_dump(fault_frame *registers){
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

void panic(const char *error,fault_frame *fault){
    kprintf("====== ERROR : KERNEL PANIC =====\n");
    kprintf("error : %s\n",error);
    kprintf("========== REGISTER DUMP ========\n");
    if(fault){
        kprintf("rax : 0x%x\tr8  : 0x%x\n",fault->rax,fault->r8);
        kprintf("rbx : 0x%x\tr9  : 0x%x\n",fault->rbx,fault->r9);
        kprintf("rcx : 0x%x\tr10 : 0x%x\n",fault->rcx,fault->r10);
        kprintf("rdx : 0x%x\tr11 : 0x%x\n",fault->rdx,fault->r11);
        kprintf("rsi : 0x%x\tr12 : 0x%x\n",fault->rsi,fault->r12);
        kprintf("rdi : 0x%x\tr13 : 0x%x\n",fault->rdi,fault->r13);
        kprintf("rbp : 0x%x\tr14 : 0x%x\n",fault->rbp,fault->r14);
        kprintf("rsp : 0x%x\tr15 : 0x%x\n",fault->rsp,fault->r15);
        kprintf("======= SPECIAL REGISTERS =======\n");
        asm("mov %%cr2 ,%%rax" : "=a"(fault->cr2) : );
        asm("mov %%cr3 ,%%rax" : "=a"(fault->cr3) : );
        kprintf("cr2 : 0x%x\tcr3 : 0x%x\n",fault->cr2,fault->cr3);
    }else{
        kprintf("unavalible\n");
    }
    halt();
}