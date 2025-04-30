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
    push ebp
    push edi
    push esi
    push edx
    push ecx
    push ebx
    push eax
    mov eax, cr3
    push eax
    mov eax, cr2
    push eax
    mov edi, esp
    call exception_handler
    pop eax
    pop eax
    pop eax
    pop ebx
    pop ecx
    pop edx
    pop esi
    pop edi
    pop ebp
    add esp,16
    iret
extern isr_ignore
isr_ignore:
	iret