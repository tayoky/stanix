%macro isr_err_stub_no_code 2
    global %1
    %1:
        push 0
        push %2
        jmp isr_err_stub_base
%endmacro
%macro isr_err_stub_code 2
    global %1
    %1:
        push %2
        jmp isr_err_stub_base
%endmacro
section .text
extern exception_handler
isr_err_stub_no_code divide_exception, 0
isr_err_stub_no_code overflow_exception, 4
isr_err_stub_no_code invalid_op_exception, 6
isr_err_stub_code invalid_tss_exception, 10
isr_err_stub_code global_fault_exception, 13
isr_err_stub_code pagefault_exception, 14
isr_err_stub_no_code isr128, 128

isr_err_stub_base:
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
    call exception_handler
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
extern isr_ignore
isr_ignore:
	iretq