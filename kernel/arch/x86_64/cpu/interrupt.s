%macro isr_no_code 2
    global %1
    %1:
        push %2
        push %2
        jmp isr_base
%endmacro
%macro isr_code 2
    global %1
    %1:
        push %2
        jmp isr_base
%endmacro
section .text
extern isr_handler


isr_no_code isr0, 0
isr_no_code isr1, 1
isr_no_code isr2, 2
isr_no_code isr3, 3
isr_no_code isr4, 4
isr_no_code isr5, 5
isr_no_code isr6, 6
isr_no_code isr7, 7
isr_code isr8, 8
isr_no_code isr9, 9
isr_code isr10, 10
isr_code isr11, 11
isr_code isr12, 12
isr_code isr13, 13
isr_code isr14, 14
isr_no_code isr15, 15
isr_no_code isr16, 16
isr_code isr17, 17
isr_no_code isr18, 18
isr_no_code isr19, 19
isr_no_code isr20, 20
isr_code isr21, 21

; futures interrupts
%assign i 22
%rep (32 - 22)
    isr_no_code isr%+i, i
    %assign i i+1
%endrep

%assign i 32
%rep (256 - 32)
    isr_no_code isr%+i, i
    %assign i i+1
%endrep

isr_base:
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

    ;save cr2 and cr3
    mov rax, cr3
    push rax
    mov rax, cr2
    push rax
    
    ;save segs
    xor rax, rax
    mov ax, ds
    push rax
    mov ax, es
    push rax
    mov ax, fs
    push rax
    mov ax, gs
    push rax
    
    
    mov rdi, rsp
    cld
    and rsp, ~0xf

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov gs, ax

    call isr_handler

    ;load segs
    pop rax
    mov gs, ax
    pop rax
    ;mov fs, ax
    pop rax
    mov es, ax
    pop rax
    mov ds, ax
    
    ;skip cr2 and cr3
    add rsp, 16

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
    