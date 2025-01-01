%macro isr_err_stub 1
    push rax
    mov rax, %+%1
    call exception_handler
    pop rax
    iretq
%endmacro
section .text
global divide_exception
global overflow_exception
global pagefault_exception
extern exception_handler

divide_exception:
isr_err_stub 0
overflow_exception:
isr_err_stub 4
pagefault_exception:
isr_err_stub 14