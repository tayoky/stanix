bits 64
section .text
global _start
_start:
	mov rax, 1
	mov rbx, 3
	add rax,rbx
	;now exit
	mov rax, 0
	mov rdi, 1
	int 80h