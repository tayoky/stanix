extern irq_handler

%macro irq 1
global irq%1
    irq%1:
        push %1
        push %1
        jmp irq_base
%endmacro

irq 0
irq 1
irq 2
irq 3
irq 4
irq 5
irq 6
irq 7
irq 8
irq 9
irq 10
irq 11
irq 12
irq 13
irq 14
irq 15

irq_base:
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax
    mov rax, cr3
    push rax
    mov rax, cr2
    push rax
    mov rdi, rsp
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    cld
    call irq_handler
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    pop rax
    pop rax
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    add rsp,16
    iretq