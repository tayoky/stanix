%macro isr_err_stub 2
    global %1
    %1:
        push rax
        mov rax, %2
        call exception_handler
        pop rax
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