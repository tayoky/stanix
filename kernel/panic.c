#include "panic.h"
#include "print.h"
#include "asm.h"
#include "kernel.h"
#include "scheduler.h"

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
    disable_interrupt();
    pid_t pid = 0;
    if(kernel->can_task_switch){
        pid = get_current_proc()->pid;
    }
    kprintf(COLOR_RED "====== ERROR : KERNEL PANIC =====\n");
    kprintf("error : %s\n",error);
    kprintf("code : 0x%lx\n",fault->err_code);
    kprintf("============= INFOS =============\n");
    kprintf("pid : %ld\n",pid);
    kprintf("========== REGISTER DUMP ========\n");
    if(fault){
        kprintf("rax : 0x%lx\tr8  : 0x%lx\n",fault->rax,fault->r8);
        kprintf("rbx : 0x%lx\tr9  : 0x%lx\n",fault->rbx,fault->r9);
        kprintf("rcx : 0x%lx\tr10 : 0x%lx\n",fault->rcx,fault->r10);
        kprintf("rdx : 0x%lx\tr11 : 0x%lx\n",fault->rdx,fault->r11);
        kprintf("rsi : 0x%lx\tr12 : 0x%lx\n",fault->rsi,fault->r12);
        kprintf("rdi : 0x%lx\tr13 : 0x%lx\n",fault->rdi,fault->r13);
        kprintf("rbp : 0x%lx\tr14 : 0x%lx\n",fault->rbp,fault->r14);
        kprintf("rsp : 0x%lx\tr15 : 0x%lx\n",fault->rsp,fault->r15);
        kprintf("======= SPECIAL REGISTERS =======\n");
        kprintf("cr2 : 0x%lx\tcr3 : 0x%lx\n",fault->cr2,fault->cr3);
        kprintf("rip : 0x%lx\n",fault->rip);
        kprintf("============= FLAGS =============\n");
        kprintf("falgs: 0x%lx\n",fault->flags);
        kprintf("========== STACK TRACE ==========\n");
        kprintf("most recent call\n");
        kprintf("<0x%lx>\n",fault->rip);
        uint64_t *rbp = (uint64_t *)fault->rbp;
        while (rbp && *rbp && *(rbp+1)){
            kprintf("<0x%lx>\n",*(rbp+1));
            rbp = (uint64_t *)(*rbp);
        }
        
        kprintf("older call\n");

    }else{
        kprintf("unavalible\n");
    }
    kprintf(COLOR_RESET);
    halt();
}