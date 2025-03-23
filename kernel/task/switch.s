global context_switch
global context_save
global context_load
context_save: 
	;cr2
	;cr3
	mov qword[rdi + 0x10], rax
	mov qword[rdi + 0x18], rbx
	mov qword[rdi + 0x20], rcx
	mov qword[rdi + 0x28], rdx
	mov qword[rdi + 0x30], rsi
	mov qword[rdi + 0x38], rdi
	mov qword[rdi + 0x40], rbp
	mov qword[rdi + 0x48], r8 
	mov qword[rdi + 0x50], r9 
	mov qword[rdi + 0x58], r10
	mov qword[rdi + 0x60], r11
	mov qword[rdi + 0x68], r12
	mov qword[rdi + 0x70], r13
	mov qword[rdi + 0x78], r14
	mov qword[rdi + 0x80], r15
	ret

context_load:
	;cr2
	;cr3
	mov rax, qword[rdi + 0x10]
	mov rbx, qword[rdi + 0x18]
	mov rcx, qword[rdi + 0x20]
	mov rdx, qword[rdi + 0x28]
	mov rsi, qword[rdi + 0x30]
	mov rdi, qword[rdi + 0x38]
	mov rbp, qword[rdi + 0x40]
	mov r8 , qword[rdi + 0x48]
	mov r9 , qword[rdi + 0x50]
	mov r10, qword[rdi + 0x58]
	mov r11, qword[rdi + 0x60]
	mov r12, qword[rdi + 0x68]
	mov r13, qword[rdi + 0x70]
	mov r14, qword[rdi + 0x78]
	mov r15, qword[rdi + 0x80]
	ret
	
context_switch:
	;save context
	;use cr2 to store the address to return
	pop rax
	mov cr2, rax

	;save ss
	mov ax, ss
	push rax ;ss

	mov rax, rsp
	add rax, 8
	push rax
	pushf

	;save cs
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

	;reset rsp
	add rsp, 40
	;mov qword[rsp - 16], rsp
	sub rsp, 40

	iretq