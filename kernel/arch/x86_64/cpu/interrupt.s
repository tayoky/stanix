%macro isr_no_code 2
    global %1
    %1:
        push 0
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
isr_no_code divide_exception, 0
isr_no_code overflow_exception, 4
isr_no_code invalid_op_exception, 6
isr_code invalid_tss_exception, 10
isr_code global_fault_exception, 13
isr_code pagefault_exception, 14
isr_no_code isr128, 128

isr_no_code irq0 , 32
isr_no_code irq1 , 33
isr_no_code irq2 , 34
isr_no_code irq3 , 35
isr_no_code irq4 , 36
isr_no_code irq5 , 37
isr_no_code irq6 , 38
isr_no_code irq7 , 39
isr_no_code irq8 , 40
isr_no_code irq9 , 41
isr_no_code irq10, 42
isr_no_code irq11, 43
isr_no_code irq12, 44
isr_no_code irq13, 45
isr_no_code irq14, 46
isr_no_code irq15, 47

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
    mov rdi, rsp
    
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
    
    cld
    call isr_handler

    ;load segs
    pop rax
    mov gs, ax
    pop rax
    mov fs, ax
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
extern isr_ignore
isr_ignore:
	iretq