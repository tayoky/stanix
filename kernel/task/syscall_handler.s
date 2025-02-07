extern sys_exit
extern sys_getpid
extern kdebugf
section .data
syscall_table:
dq sys_exit
dq sys_getpid
syscall_table_end:
msg:
db `rax : %ld\n`, 0
section .text
global syscall_handler
syscall_handler:
	push rbx
	push rcx
	push rdx
	push rsi
	push rdi
	push rbp
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15

	mov dx, ds
	push rdx

	;restore kernel segment
	mov dx, 0x10
	mov ds, dx
	mov es, dx
	mov fs ,dx
	mov gs, dx

	;first out of bound check
	cmp rax, syscall_table_end - syscall_table
	ja sys_invalid

	;now just call the handler
	call qword[rax + syscall_table]

	;restore user segment
	pop rdx
	mov ds, dx
	mov es, dx
	mov fs ,dx
	mov gs, dx
	
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rbp
	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop rbx

sys_invalid:
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rbp
	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop rbx
	mov rax, -1
	iretq