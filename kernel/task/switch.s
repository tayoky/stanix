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
;first save rsp
mov qword[rdi + 8], rsp
mov rax, qword[rsi]
mov cr3,rax
mov rsp, qword[rsi + 8]
ret