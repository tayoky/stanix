bits 64
section .text
global _start
_start:
	;just sbrk test
	mov rax,8 ;sbrk
	mov rdi, 0x1000 ; 4096 bytes
	int 80h

	;first open the tty
	mov rax, 1
	mov rdi, path
	mov rsi, 1 ;write only mode
	int 80h

	;save fd in r15
	mov r15, rax
	
	;now write to it
	mov rax, 4
	mov rdi, r15
	mov rsi, hello
	mov rdx, 22
	int 80h

	;now close it
	mov rax, 2
	mov rdi, r15
	int 80h

	;now exit
	mov rax, 0
	mov rdi, 1
	int 80h

section .data
path:
db `dev:/tty0`,0
hello :
db `hello from program !!\n`