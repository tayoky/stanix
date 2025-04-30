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
    call irq_handler
    pop eax
    pop eax
    pop eax
    pop ebx
    pop ecx
    pop edx
    pop esi
    pop edi
    add esp, 8
    add esp,16
    iret