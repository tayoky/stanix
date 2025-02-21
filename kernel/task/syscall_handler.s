section .data
msg:
db `rax : %lx\n`, 0
section .text
extern syscall_table
extern syscall_number
extern get_current_proc
global syscall_handler
syscall_handler:
	push rdi
	push rsi
	push rax
	push rbx
	push rcx
	push rdx
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
	push rbp

	;push seg
	mov bx, ds
	push rbx

	;restore kernel segment
	mov bx, 0x10
	mov ds, bx
	mov es, bx
	mov fs ,bx
	mov gs, bx

	;get the current proc
	push rax
	push rdi
	push rsi
	push rdx
	push rcx
	call get_current_proc
	mov r15, rax
	pop rcx
	pop rdx
	pop rsi
	pop rdi
	pop rax

	;set the syscall frame
	mov qword[r15 + 16], rsp

	;first out of bound check 
	cmp rax, qword[syscall_number]
	jae sys_invalid

	;mov rdi, msg
	;mov rsi, qword[syscall_table + rax * 8]
	;call kdebugf

	;now just call the handler
	call qword[syscall_table + rax * 8]

	syscall_finish:
	;restore seg
	pop rbx
	mov ds, bx
	mov es, bx
	mov fs, bx
	mov gs, bx

	;pop all
	pop rbp
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
	pop rbx
	add rsp, 8 ;don't pop rax
	pop rsi
	pop rdi
	iretq

sys_invalid:
	mov rax, -1
	jmp syscall_finish