bits 64
section .text
global _start
_start:
	mov rax, 1
	mov rbx, 3
	add rax,rbx
	loop:
	jmp loop