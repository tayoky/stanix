%macro isr_err_stub 2
    global %1
    %1:
        push %2
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
        call exception_handler
        pop rax
        pop rax
        pop rax
        pop rbx
        pop rcx
        pop rdx
        pop rsi
        pop rdi
        pop r15
        pop r8
        pop r9
        pop r10
        pop r11
        pop r12
        pop r13
        pop r14
        pop r15
        add rsp,8
        iretq
%endmacro
section .text
extern exception_handler
isr_err_stub divide_exception, 0
isr_err_stub overflow_exception, 4
isr_err_stub invalid_op_exception, 6
isr_err_stub invalid_tss_exception, 10
isr_err_stub global_fault_exception, 13
isr_err_stub pagefault_exception, 14