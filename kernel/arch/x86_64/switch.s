extern kernel
global context_switch
context_switch:

	;save context
	;use cr2 to store the address to return
	pop rax
	mov cr2, rax

	;save ss
	xor rax, rax
	mov ax, ss
	push rax ;ss

	mov rax, rsp
	add rax, 8
	push rax
	pushf

	cli

	;save cs
	xor rax, rax
	mov ax, cs
	push rax ;cs

	;save rip
	mov rax, cr2
	push rax ;rip

	push 0
	push 0
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
	push 0 ;cr3
	push 0 ;cr2

	;first save rsp

	mov qword[rdi + 8], rsp
	mov rax, qword[rsi]
	mov cr3,rax
	mov rsp, qword[rsi + 8]

	add rsp, 16 ;skip cr2 ad cr3
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
	add rsp, 16 ;skip err code and type

	;restore ds and other segment
	push rax
	mov rax, qword[rsp + 40]

	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	pop rax
	
	iretq