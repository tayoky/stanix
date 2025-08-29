bits 64
section .text
global main
main:
;print hello
    mov rax, 4
    mov rdi, 1
    mov rsi, hello
    mov rdx, hellolen
    int 80h

;exit
    mov rax, 0
    mov rdi, 0
    int 80h

section .data
hello: db `hello world`,10
hellolen equ $ - hello